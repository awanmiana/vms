#pragma once

// P1-13 / A0 — the command-coverage check (native increment 29). Scope:
// ../command-coverage-P1-13-proposal.md.
//
// P1-13 requires "a coverage check that fails if any UI action bypasses the
// command layer." This is that check: it scans the SHIPPED QML (the caller
// reads it out of the Qt resource, so the check inspects what actually ships,
// not a stale source tree) for direct `controller.method(` call sites and
// compares each against a declared table of state-changing mutators. A mutator
// invoked directly is a violation — it would run with no capability check, no
// dangerous-action confirmation, and no audit row. A routed call reaches the
// controller only via `commander.invoke(...)` / `commander.run(...)`, so it
// simply does not name the controller method in QML at all.
//
// Exemptions are declared, categorized, and PRINTED in the report — never
// silent. They cover pure reads (reading changes nothing), continuous gesture
// recomputation (viewport/scrub frames that persist nothing; the committed
// result of the gesture is itself a command), and surfaces explicitly deferred
// to a later slice, which the report lists as known gaps.
//
// Pure C++17, Qt-free: the caller supplies file contents as strings.

#include <string>
#include <utility>
#include <vector>

namespace vms::command {

enum class Exemption {
    None,                 // must be routed through the envelope
    PureRead,             // a read: changes no state
    ContinuousViewport,   // recomputed per pan/zoom frame; persists nothing
    ContinuousTransport,  // scrub frames; the discrete verbs are commands
    DeferredSlice         // a known, named gap scheduled for a later slice
};
const char* ExemptionName(Exemption e);

// One QML-callable controller method on the declared surface.
struct MutatorSpec {
    std::string object;      // QML object/context property, e.g. "devicesCtrl"
    std::string method;      // e.g. "onboard"
    Exemption exemption = Exemption::None;
    std::string reason;      // why it is exempt (required when exempt)
};

// The declared surface: every state-changing controller method reachable from
// QML, plus the categorized exemptions. Adding a Q_INVOKABLE mutator without
// adding it here is itself a gap — `--coverage-check` prints the declared
// count so the surface is auditable.
std::vector<MutatorSpec> DeclaredMutators();

struct CoverageViolation {
    std::string file;
    int line = 0;
    std::string object;
    std::string method;
    std::string text;   // the offending source line, trimmed
};

struct CoverageNote {
    std::string object;
    std::string method;
    Exemption exemption = Exemption::None;
    std::string reason;
    int sites = 0;      // direct call sites found for this exempt mutator
};

struct CoverageReport {
    std::vector<CoverageViolation> violations;
    std::vector<CoverageNote> notes;   // exempt mutators actually found
    int filesScanned = 0;
    int declaredMutators = 0;
    int directCallSites = 0;   // total direct calls found (violations + exempt)
    bool ok() const { return violations.empty(); }
};

// Scan the given (filename, contents) pairs against the declared surface.
CoverageReport CheckCoverage(
    const std::vector<std::pair<std::string, std::string>>& files,
    const std::vector<MutatorSpec>& declared);

// Human-readable report (printed by --coverage-check).
std::string FormatCoverageReport(const CoverageReport& r);

} // namespace vms::command
