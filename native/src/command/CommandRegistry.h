#pragma once

// P1-12 / A0 — the unified command envelope (native increment 25). Scope:
// ../command-envelope-P1-12-proposal.md.
//
// One validated command per action (the A0 mandate): a CommandRegistry holds
// the machine-readable spec of every action the software can perform — stable
// id, typed parameters, required capability, dangerous flag — and every
// invocation flows through ONE gate: validate -> capability check -> dangerous
// confirmation -> execute -> deterministic result mapping, with an audit
// record for every attempt, refused or executed. UI clicks, typed text, voice
// (later), and the external AI-agent API (P1-13) all become callers of this
// same envelope, so no path can bypass validation, capability, confirmation,
// or audit.
//
// Pure C++17, Qt-free, clock-free (the audit sink stamps time), unit-tested by
// vms_cmdtest/CTest. Durable audit storage is P1-06; real RBAC is P1-02/P1-03
// — the envelope checks capability MEMBERSHIP so role separation later needs
// no rewrite (P0-01A: the Administrator session simply grants everything).

#include <functional>
#include <map>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace vms::command {

// A parameter value. Text parsing and API layers both normalize to this.
using Value = std::variant<std::int64_t, double, std::string, bool>;
using Args = std::map<std::string, Value>;

enum class ParamType { Int, Number, String, Bool, Enum };

struct ParamSpec {
    std::string name;
    ParamType type = ParamType::String;
    bool required = true;
    // For Int/Number: inclusive range (ignored when min > max).
    double min = 1.0;
    double max = 0.0;
    // For Enum: the allowed values.
    std::vector<std::string> oneOf;
    // A secret (password, token). Its VALUE is never rendered into an audit
    // record, a log line, or the catalog — only "***". The credential itself
    // still reaches the handler, which stores it through the OS secret store;
    // the audit proves the action happened without leaking what was set.
    bool secret = false;
};

struct CommandSpec {
    std::string id;           // stable, e.g. "workspace.focus"
    std::string title;        // human-readable
    std::string capability;   // required capability, e.g. "workspace.control"
    bool dangerous = false;   // requires an explicit confirmation
    std::vector<ParamSpec> params;
};

// Deterministic outcome codes (the A0 "result mapping"). `Ok` is the only
// success; everything else is an honest typed refusal or failure.
enum class Outcome {
    Ok,
    UnknownCommand,
    MissingParam,
    BadParam,          // wrong type / out of range / not in enum
    CapabilityDenied,
    NeedsConfirm,      // dangerous command without an explicit confirmation
    Failed             // the handler itself reported failure
};
const char* OutcomeName(Outcome o);

struct CommandResult {
    Outcome outcome = Outcome::Failed;
    std::string message;   // human-readable detail (refusal reason or effect)
    explicit operator bool() const { return outcome == Outcome::Ok; }
};

// Every attempt produces one audit record — refused attempts included, because
// "who tried what and was stopped" is as auditable as "who did what" (P1-06
// makes this durable later; the sink receives it now).
struct AuditRecord {
    std::string commandId;   // as requested (may be unknown)
    std::string argsText;    // canonical "name=value" rendering
    Outcome outcome = Outcome::Failed;
    std::string message;
};
using AuditSink = std::function<void(const AuditRecord&)>;

// A handler executes an ALREADY-validated invocation and reports success or a
// failure message. Handlers never see unvalidated input.
using Handler = std::function<CommandResult(const Args&)>;

class CommandRegistry {
public:
    // Register a command. Returns false (and ignores the call) on a duplicate
    // or empty id — the catalog must stay unambiguous.
    bool add(const CommandSpec& spec, Handler handler);

    // The single gate. `capabilities` is the caller's granted set;
    // `confirmed` is the explicit dangerous-action confirmation.
    CommandResult invoke(const std::string& id, const Args& args,
                         const std::set<std::string>& capabilities,
                         bool confirmed = false) const;

    // Deterministic text leg (P1-12): "focus 5", "quality 3 thumb",
    // "device.remove cam-1 confirm". The first token is the command id
    // (a bare verb also matches "workspace.<verb>"), remaining tokens bind to
    // the declared params IN ORDER, and a literal trailing "confirm" sets the
    // confirmation flag. Returns the same typed refusals as invoke().
    CommandResult invokeText(const std::string& line,
                             const std::set<std::string>& capabilities) const;

    // Machine-readable catalog (JSON array of specs) — the P1-13 discovery
    // seed an external agent will read.
    std::string catalogJson() const;
    // Capability-filtered catalog for a scoped external identity. A client is
    // never advertised commands it cannot invoke.
    std::string catalogJson(const std::set<std::string>& capabilities) const;

    // Ordered command ids (for the palette's command list).
    std::vector<std::string> ids() const;
    const CommandSpec* find(const std::string& id) const;

    void setAuditSink(AuditSink sink) { audit_ = std::move(sink); }

private:
    CommandResult refuse(const std::string& id, const Args& args,
                         Outcome outcome, const std::string& message) const;
    void record(const std::string& id, const Args& args,
                const CommandResult& r) const;

    struct Entry {
        CommandSpec spec;
        Handler handler;
    };
    std::vector<Entry> entries_;       // registration order (stable catalog)
    std::map<std::string, int> byId_;  // id -> index
    AuditSink audit_;
};

// Render args canonically ("tile=5 tier=thumb") for audit and messages, with
// secrets redacted to "***". `spec` may be null (an unknown command); a
// name-based backstop still redacts password/secret/token/credential so an
// unvalidated call can never leak one either.
std::string ArgsText(const Args& args, const CommandSpec* spec = nullptr);

// True when a parameter name must be treated as a secret even without a spec.
bool IsSecretParamName(const std::string& name);

} // namespace vms::command
