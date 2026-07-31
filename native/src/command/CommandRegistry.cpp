#include "command/CommandRegistry.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace vms::command {

const char* OutcomeName(Outcome o) {
    switch (o) {
        case Outcome::Ok:               return "ok";
        case Outcome::UnknownCommand:   return "unknown-command";
        case Outcome::MissingParam:     return "missing-param";
        case Outcome::BadParam:         return "bad-param";
        case Outcome::CapabilityDenied: return "capability-denied";
        case Outcome::NeedsConfirm:     return "needs-confirm";
        case Outcome::Failed:           return "failed";
    }
    return "?";
}

namespace {

std::string valueText(const Value& v) {
    if (std::holds_alternative<std::int64_t>(v))
        return std::to_string(std::get<std::int64_t>(v));
    if (std::holds_alternative<double>(v)) {
        std::ostringstream os;
        os << std::get<double>(v);
        return os.str();
    }
    if (std::holds_alternative<bool>(v))
        return std::get<bool>(v) ? "true" : "false";
    return std::get<std::string>(v);
}

const char* paramTypeName(ParamType t) {
    switch (t) {
        case ParamType::Int:    return "int";
        case ParamType::Number: return "number";
        case ParamType::String: return "string";
        case ParamType::Bool:   return "bool";
        case ParamType::Enum:   return "enum";
    }
    return "?";
}

// JSON string escaping for the catalog (ids/titles are ASCII by convention,
// but escape defensively).
std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

} // namespace

bool IsSecretParamName(const std::string& name) {
    std::string lower;
    lower.reserve(name.size());
    for (char c : name)
        lower += static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));
    return lower.find("password") != std::string::npos ||
           lower.find("secret") != std::string::npos ||
           lower.find("token") != std::string::npos ||
           lower.find("credential") != std::string::npos ||
           lower.find("passphrase") != std::string::npos;
}

std::string ArgsText(const Args& args, const CommandSpec* spec) {
    std::string out;
    for (const auto& [name, value] : args) {
        if (!out.empty()) out += ' ';
        bool secret = IsSecretParamName(name);   // backstop
        if (spec) {
            for (const ParamSpec& p : spec->params)
                if (p.name == name && p.secret) { secret = true; break; }
        }
        out += name + '=' + (secret ? std::string("***") : valueText(value));
    }
    return out;
}

bool CommandRegistry::add(const CommandSpec& spec, Handler handler) {
    if (spec.id.empty() || byId_.count(spec.id) || !handler) return false;
    byId_[spec.id] = static_cast<int>(entries_.size());
    entries_.push_back({spec, std::move(handler)});
    return true;
}

void CommandRegistry::record(const std::string& id, const Args& args,
                             const CommandResult& r) const {
    if (!audit_) return;
    AuditRecord rec;
    rec.commandId = id;
    // Redact secrets against the command's own spec (with a name-based
    // backstop when the id is unknown): the audit proves WHAT was done, never
    // leaks a credential into the log.
    rec.argsText = ArgsText(args, find(id));
    rec.outcome = r.outcome;
    rec.message = r.message;
    audit_(rec);
}

CommandResult CommandRegistry::refuse(const std::string& id, const Args& args,
                                      Outcome outcome,
                                      const std::string& message) const {
    CommandResult r;
    r.outcome = outcome;
    r.message = message;
    record(id, args, r);
    return r;
}

CommandResult CommandRegistry::invoke(const std::string& id, const Args& args,
                                      const std::set<std::string>& capabilities,
                                      bool confirmed) const {
    const auto it = byId_.find(id);
    if (it == byId_.end())
        return refuse(id, args, Outcome::UnknownCommand,
                      "unknown command '" + id + "'");
    const Entry& e = entries_[static_cast<size_t>(it->second)];

    // 1) Validate the arguments against the spec — nothing executes on a
    //    mismatch, and the refusal names the exact parameter.
    for (const ParamSpec& p : e.spec.params) {
        const auto a = args.find(p.name);
        if (a == args.end()) {
            if (p.required)
                return refuse(id, args, Outcome::MissingParam,
                              "missing required parameter '" + p.name + "'");
            continue;
        }
        const Value& v = a->second;
        switch (p.type) {
            case ParamType::Int: {
                if (!std::holds_alternative<std::int64_t>(v))
                    return refuse(id, args, Outcome::BadParam,
                                  "'" + p.name + "' must be an integer");
                const double d = static_cast<double>(std::get<std::int64_t>(v));
                if (p.min <= p.max && (d < p.min || d > p.max))
                    return refuse(id, args, Outcome::BadParam,
                                  "'" + p.name + "' out of range");
                break;
            }
            case ParamType::Number: {
                double d = 0;
                if (std::holds_alternative<double>(v))
                    d = std::get<double>(v);
                else if (std::holds_alternative<std::int64_t>(v))
                    d = static_cast<double>(std::get<std::int64_t>(v));
                else
                    return refuse(id, args, Outcome::BadParam,
                                  "'" + p.name + "' must be a number");
                if (p.min <= p.max && (d < p.min || d > p.max))
                    return refuse(id, args, Outcome::BadParam,
                                  "'" + p.name + "' out of range");
                break;
            }
            case ParamType::Bool:
                if (!std::holds_alternative<bool>(v))
                    return refuse(id, args, Outcome::BadParam,
                                  "'" + p.name + "' must be true or false");
                break;
            case ParamType::String:
                if (!std::holds_alternative<std::string>(v) ||
                    std::get<std::string>(v).empty())
                    return refuse(id, args, Outcome::BadParam,
                                  "'" + p.name + "' must be a non-empty string");
                break;
            case ParamType::Enum: {
                if (!std::holds_alternative<std::string>(v))
                    return refuse(id, args, Outcome::BadParam,
                                  "'" + p.name + "' must be one of the listed values");
                const std::string& s = std::get<std::string>(v);
                bool found = false;
                for (const std::string& o : p.oneOf)
                    if (o == s) { found = true; break; }
                if (!found) {
                    std::string allowed;
                    for (const std::string& o : p.oneOf) {
                        if (!allowed.empty()) allowed += '|';
                        allowed += o;
                    }
                    return refuse(id, args, Outcome::BadParam,
                                  "'" + p.name + "' must be one of " + allowed);
                }
                break;
            }
        }
    }
    // Unknown extra parameters are refused too: a caller sending arguments the
    // spec does not declare is a caller whose intent we cannot validate.
    for (const auto& [name, value] : args) {
        (void)value;
        bool declared = false;
        for (const ParamSpec& p : e.spec.params)
            if (p.name == name) { declared = true; break; }
        if (!declared)
            return refuse(id, args, Outcome::BadParam,
                          "unknown parameter '" + name + "'");
    }

    // 2) Capability membership (permission-ready: Administrator grants all).
    if (!e.spec.capability.empty() && !capabilities.count(e.spec.capability))
        return refuse(id, args, Outcome::CapabilityDenied,
                      "requires capability '" + e.spec.capability + "'");

    // 3) Dangerous-action confirmation (A0: destructive/physical actions are
    //    never one accidental token away).
    if (e.spec.dangerous && !confirmed)
        return refuse(id, args, Outcome::NeedsConfirm,
                      "'" + id + "' is destructive — append 'confirm' to proceed");

    // 4) Execute the validated invocation and map the outcome.
    CommandResult r = e.handler(args);
    if (r.outcome != Outcome::Ok && r.outcome != Outcome::Failed)
        r.outcome = Outcome::Failed;   // handlers only report ok/failed
    record(id, args, r);
    return r;
}

CommandResult CommandRegistry::invokeText(
    const std::string& line, const std::set<std::string>& capabilities) const {
    // Tokenize on whitespace (deterministic; no quoting in this slice — ids
    // and enum values are single tokens, free strings bind last).
    std::vector<std::string> tokens;
    std::istringstream is(line);
    for (std::string t; is >> t;) tokens.push_back(t);
    if (tokens.empty())
        return refuse("", {}, Outcome::UnknownCommand, "empty command");

    // Resolve the id: exact match first, then the "workspace.<verb>" shorthand.
    std::string id = tokens[0];
    if (!byId_.count(id) && byId_.count("workspace." + id))
        id = "workspace." + id;
    const auto it = byId_.find(id);
    if (it == byId_.end())
        return refuse(tokens[0], {}, Outcome::UnknownCommand,
                      "unknown command '" + tokens[0] + "'");
    const CommandSpec& spec = entries_[static_cast<size_t>(it->second)].spec;

    // A literal trailing "confirm" is the explicit dangerous-action consent.
    bool confirmed = false;
    if (tokens.size() >= 2 && tokens.back() == "confirm") {
        confirmed = true;
        tokens.pop_back();
    }

    // Bind remaining tokens to the declared params in order, typed per spec.
    Args args;
    size_t ti = 1;
    for (const ParamSpec& p : spec.params) {
        if (ti >= tokens.size()) break;   // missing-required surfaces in invoke()
        const std::string& tok = tokens[ti++];
        switch (p.type) {
            case ParamType::Int: {
                char* end = nullptr;
                const long long n = std::strtoll(tok.c_str(), &end, 10);
                if (!end || *end != '\0')
                    return refuse(id, args, Outcome::BadParam,
                                  "'" + p.name + "' must be an integer");
                args[p.name] = static_cast<std::int64_t>(n);
                break;
            }
            case ParamType::Number: {
                char* end = nullptr;
                const double d = std::strtod(tok.c_str(), &end);
                if (!end || *end != '\0')
                    return refuse(id, args, Outcome::BadParam,
                                  "'" + p.name + "' must be a number");
                args[p.name] = d;
                break;
            }
            case ParamType::Bool:
                if (tok == "true" || tok == "on" || tok == "1")
                    args[p.name] = true;
                else if (tok == "false" || tok == "off" || tok == "0")
                    args[p.name] = false;
                else
                    return refuse(id, args, Outcome::BadParam,
                                  "'" + p.name + "' must be on or off");
                break;
            case ParamType::String:
            case ParamType::Enum:
                args[p.name] = tok;
                break;
        }
    }
    if (ti < tokens.size())
        return refuse(id, args, Outcome::BadParam,
                      "unexpected extra argument '" + tokens[ti] + "'");

    return invoke(id, args, capabilities, confirmed);
}

std::string CommandRegistry::catalogJson() const {
    std::ostringstream os;
    os << "[";
    bool firstCmd = true;
    for (const Entry& e : entries_) {
        if (!firstCmd) os << ",";
        firstCmd = false;
        os << "{\"id\":\"" << jsonEscape(e.spec.id) << "\","
           << "\"title\":\"" << jsonEscape(e.spec.title) << "\","
           << "\"capability\":\"" << jsonEscape(e.spec.capability) << "\","
           << "\"dangerous\":" << (e.spec.dangerous ? "true" : "false") << ","
           << "\"params\":[";
        bool firstP = true;
        for (const ParamSpec& p : e.spec.params) {
            if (!firstP) os << ",";
            firstP = false;
            os << "{\"name\":\"" << jsonEscape(p.name) << "\","
               << "\"type\":\"" << paramTypeName(p.type) << "\","
               << "\"required\":" << (p.required ? "true" : "false");
            // Tell a machine client which values must never be logged.
            if (p.secret || IsSecretParamName(p.name)) os << ",\"secret\":true";
            if (p.type == ParamType::Enum) {
                os << ",\"oneOf\":[";
                bool firstO = true;
                for (const std::string& o : p.oneOf) {
                    if (!firstO) os << ",";
                    firstO = false;
                    os << "\"" << jsonEscape(o) << "\"";
                }
                os << "]";
            } else if ((p.type == ParamType::Int || p.type == ParamType::Number)
                       && p.min <= p.max) {
                os << ",\"min\":" << p.min << ",\"max\":" << p.max;
            }
            os << "}";
        }
        os << "]}";
    }
    os << "]";
    return os.str();
}

std::vector<std::string> CommandRegistry::ids() const {
    std::vector<std::string> out;
    out.reserve(entries_.size());
    for (const Entry& e : entries_) out.push_back(e.spec.id);
    return out;
}

const CommandSpec* CommandRegistry::find(const std::string& id) const {
    const auto it = byId_.find(id);
    if (it == byId_.end()) return nullptr;
    return &entries_[static_cast<size_t>(it->second)].spec;
}

} // namespace vms::command
