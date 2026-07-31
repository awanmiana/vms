// P1-12 / A0 command-envelope self-check (native increment 25). No Qt, no
// display. Exercises the single validated gate every action must flow through:
// spec validation (typed refusals, nothing executes), capability membership,
// dangerous-action confirmation, deterministic result mapping, the text leg,
// the audit trail (refusals included), and the machine-readable catalog.
// Registered with CTest as `command_selfcheck`.

#include "command/CommandRegistry.h"

#include <iostream>
#include <string>
#include <vector>

using namespace vms::command;

namespace {

int failures = 0;

void check(bool cond, const std::string& what) {
    std::cout << (cond ? "  ok  " : "  FAIL ") << what << "\n";
    if (!cond) ++failures;
}

} // namespace

int main() {
    std::cout << "vms_cmdtest — P1-12/A0 command-envelope self-check\n";

    CommandRegistry reg;
    std::vector<AuditRecord> audit;
    reg.setAuditSink([&](const AuditRecord& r) { audit.push_back(r); });

    // A plain command with an int-range param and an enum param.
    int focusCalls = 0, lastTile = -1;
    std::string lastTier;
    check(reg.add({"workspace.focus", "Focus a tile", "workspace.control",
                   false,
                   {{"tile", ParamType::Int, true, 0, 63}}},
                  [&](const Args& a) {
                      ++focusCalls;
                      lastTile = static_cast<int>(
                          std::get<std::int64_t>(a.at("tile")));
                      return CommandResult{Outcome::Ok, "focused"};
                  }),
          "register workspace.focus");
    check(reg.add({"workspace.quality", "Set a tile's quality ceiling",
                   "workspace.control", false,
                   {{"tile", ParamType::Int, true, 0, 63},
                    {"tier", ParamType::Enum, true, 1, 0,
                     {"main", "sub", "thumb", "off"}}}},
                  [&](const Args& a) {
                      lastTier = std::get<std::string>(a.at("tier"));
                      return CommandResult{Outcome::Ok, "set"};
                  }),
          "register workspace.quality");
    // A dangerous command (the confirm gate) with a required string.
    int removed = 0;
    check(reg.add({"device.remove", "Remove a device", "devices.manage", true,
                   {{"id", ParamType::String, true}}},
                  [&](const Args&) {
                      ++removed;
                      return CommandResult{Outcome::Ok, "removed"};
                  }),
          "register device.remove (dangerous)");
    // A handler that itself fails (result mapping).
    check(reg.add({"replay.start", "Instant replay", "playback.review", false,
                   {{"seconds", ParamType::Int, true, 1, 3600}}},
                  [&](const Args&) {
                      return CommandResult{Outcome::Failed,
                                           "no recording DB attached"};
                  }),
          "register replay.start (failing handler)");
    check(!reg.add({"workspace.focus", "dup", "", false, {}},
                   [](const Args&) { return CommandResult{Outcome::Ok, ""}; }),
          "duplicate id is rejected (catalog stays unambiguous)");

    const std::set<std::string> admin = {"workspace.control", "devices.manage",
                                         "playback.review"};
    const std::set<std::string> viewerOnly = {"playback.review"};

    // --- 1) Validation: typed refusals, nothing executes -------------------
    CommandResult r = reg.invoke("no.such", {}, admin);
    check(r.outcome == Outcome::UnknownCommand, "unknown command -> typed refusal");
    r = reg.invoke("workspace.focus", {}, admin);
    check(r.outcome == Outcome::MissingParam && focusCalls == 0,
          "missing required param refused, handler not run");
    r = reg.invoke("workspace.focus", {{"tile", std::string("five")}}, admin);
    check(r.outcome == Outcome::BadParam && focusCalls == 0,
          "wrong type refused, handler not run");
    r = reg.invoke("workspace.focus", {{"tile", std::int64_t{99}}}, admin);
    check(r.outcome == Outcome::BadParam && focusCalls == 0,
          "out-of-range refused, handler not run");
    r = reg.invoke("workspace.quality",
                   {{"tile", std::int64_t{3}}, {"tier", std::string("ultra")}},
                   admin);
    check(r.outcome == Outcome::BadParam, "bad enum value refused");
    r = reg.invoke("workspace.focus",
                   {{"tile", std::int64_t{1}}, {"extra", std::string("x")}},
                   admin);
    check(r.outcome == Outcome::BadParam && focusCalls == 0,
          "undeclared extra parameter refused");

    // --- 2) Capability membership ------------------------------------------
    r = reg.invoke("workspace.focus", {{"tile", std::int64_t{5}}}, viewerOnly);
    check(r.outcome == Outcome::CapabilityDenied && focusCalls == 0,
          "missing capability -> capability-denied, handler not run");
    r = reg.invoke("workspace.focus", {{"tile", std::int64_t{5}}}, admin);
    check(static_cast<bool>(r) && focusCalls == 1 && lastTile == 5,
          "granted capability -> validated invocation executes");

    // --- 3) Dangerous-action confirmation ----------------------------------
    r = reg.invoke("device.remove", {{"id", std::string("cam-1")}}, admin);
    check(r.outcome == Outcome::NeedsConfirm && removed == 0,
          "dangerous without confirm -> needs-confirm, nothing removed");
    r = reg.invoke("device.remove", {{"id", std::string("cam-1")}}, admin,
                   /*confirmed=*/true);
    check(static_cast<bool>(r) && removed == 1,
          "dangerous with explicit confirm executes");

    // --- 4) Result mapping for a failing handler ---------------------------
    r = reg.invoke("replay.start", {{"seconds", std::int64_t{30}}}, admin);
    check(r.outcome == Outcome::Failed && !r.message.empty(),
          "handler failure maps to 'failed' with the honest reason");

    // --- 5) The deterministic text leg --------------------------------------
    r = reg.invokeText("focus 7", admin);
    check(static_cast<bool>(r) && lastTile == 7,
          "text 'focus 7' resolves the workspace shorthand and executes");
    r = reg.invokeText("workspace.quality 3 thumb", admin);
    check(static_cast<bool>(r) && lastTier == "thumb",
          "text binds positional args in declared order (int + enum)");
    r = reg.invokeText("focus abc", admin);
    check(r.outcome == Outcome::BadParam, "text with a non-integer refused");
    r = reg.invokeText("focus 1 2", admin);
    check(r.outcome == Outcome::BadParam, "text with extra tokens refused");
    r = reg.invokeText("device.remove cam-2", admin);
    check(r.outcome == Outcome::NeedsConfirm,
          "text dangerous command without 'confirm' refused");
    r = reg.invokeText("device.remove cam-2 confirm", admin);
    check(static_cast<bool>(r) && removed == 2,
          "trailing 'confirm' token is the explicit consent");
    r = reg.invokeText("", admin);
    check(r.outcome == Outcome::UnknownCommand, "empty text refused honestly");

    // --- 6) Audit: every attempt recorded, refusals included ---------------
    check(!audit.empty(), "audit sink received records");
    {
        int refusals = 0, oks = 0;
        for (const AuditRecord& a : audit) {
            if (a.outcome == Outcome::Ok) ++oks;
            else ++refusals;
        }
        check(refusals >= 10 && oks >= 4,
              "refused AND executed attempts are both audited");
        // The most recent EXECUTED device.remove (the empty-text refusal above
        // was audited after it — refusals land in the trail too).
        bool found = false;
        for (auto it = audit.rbegin(); it != audit.rend(); ++it) {
            if (it->commandId == "device.remove" && it->outcome == Outcome::Ok) {
                found = it->argsText.find("id=cam-2") != std::string::npos;
                break;
            }
        }
        check(found, "audit record carries id, canonical args, and outcome");
    }

    // --- 6b) Secrets are NEVER rendered into an audit record ----------------
    // The credential still reaches the handler (it must, to be stored); what
    // must never leak is its value into a log the audit makes durable.
    {
        std::string sawPassword;
        CommandRegistry sec;
        sec.setAuditSink([&](const AuditRecord& r) { sawPassword = r.argsText; });
        sec.add({"device.onboardX", "Onboard", "devices.manage", false,
                 {{"id", ParamType::String, true},
                  {"password", ParamType::String, true, 1, 0, {}, true}}},
                [&](const Args& a) {
                    // the handler DOES receive the real secret
                    return std::get<std::string>(a.at("password")) == "s3cret"
                               ? CommandResult{Outcome::Ok, "stored"}
                               : CommandResult{Outcome::Failed, "lost the secret"};
                });
        const CommandResult r2 = sec.invoke(
            "device.onboardX",
            {{"id", std::string("cam-1")}, {"password", std::string("s3cret")}},
            {"devices.manage"});
        check(static_cast<bool>(r2),
              "the handler still receives the real credential");
        check(sawPassword.find("s3cret") == std::string::npos &&
                  sawPassword.find("password=***") != std::string::npos,
              "a secret parameter is REDACTED in the audit record");
        check(sawPassword.find("id=cam-1") != std::string::npos,
              "non-secret arguments are still recorded in full");
        check(sec.catalogJson().find("\"secret\":true") != std::string::npos,
              "the catalog marks secret parameters for machine clients");
    }
    // Backstop: an unknown command still cannot leak a secret-looking arg.
    {
        std::string text;
        CommandRegistry bare;
        bare.setAuditSink([&](const AuditRecord& r) { text = r.argsText; });
        bare.invoke("does.not.exist", {{"password", std::string("hunter2")}}, {});
        check(text.find("hunter2") == std::string::npos,
              "an unvalidated call cannot leak a secret either (name backstop)");
    }

    // --- 7) The machine-readable catalog ------------------------------------
    const std::string cat = reg.catalogJson();
    check(cat.find("\"id\":\"workspace.focus\"") != std::string::npos &&
              cat.find("\"id\":\"device.remove\"") != std::string::npos,
          "catalog lists the registered commands");
    check(cat.find("\"dangerous\":true") != std::string::npos,
          "catalog marks dangerous commands");
    check(cat.find("\"oneOf\":[\"main\",\"sub\",\"thumb\",\"off\"]") !=
              std::string::npos,
          "catalog carries enum domains for machine discovery");
    check(cat.find("\"min\":0") != std::string::npos &&
              cat.find("\"max\":63") != std::string::npos,
          "catalog carries numeric ranges");
    check(reg.ids().size() == 4 && reg.ids()[0] == "workspace.focus",
          "ids() preserves registration order (stable catalog)");
    check(reg.find("device.remove") != nullptr &&
              reg.find("device.remove")->dangerous,
          "find() returns the spec");

    if (failures == 0) {
        std::cout << "PASS: validation refusals, capability gate, dangerous "
                     "confirmation, text leg, audit trail, and catalog "
                     "verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}
