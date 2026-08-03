#include "command/CoverageCheck.h"

#include <cctype>
#include <sstream>

namespace vms::command {

const char* ExemptionName(Exemption e) {
    switch (e) {
        case Exemption::None:                return "must-route";
        case Exemption::PureRead:            return "pure-read";
        case Exemption::ContinuousViewport:  return "continuous-viewport";
        case Exemption::ContinuousTransport: return "continuous-transport";
        case Exemption::DeferredSlice:       return "deferred-slice";
    }
    return "?";
}

std::vector<MutatorSpec> DeclaredMutators() {
    return {
        // --- Live workspace (WorkspaceController, QML `governor`) -----------
        {"governor", "focusTile"},
        {"governor", "setPriority"},
        {"governor", "setDesiredTier"},
        {"governor", "setTileCount"},
        {"governor", "setAutoSweep"},
        {"governor", "setTilePos"},
        {"governor", "zoomLevelName", Exemption::PureRead,
         "reads the zoom level for the header; changes no state"},
        {"governor", "updateSpatialViewport", Exemption::ContinuousViewport,
         "recomputed every pan/zoom frame from the viewport; persists nothing. "
         "The committed result of the gesture is workspace.place"},

        // --- Device management (DeviceController, QML `devicesCtrl`) --------
        {"devicesCtrl", "onboard"},
        {"devicesCtrl", "onboardRecorder"},
        {"devicesCtrl", "onboardDiscovered"},
        {"devicesCtrl", "startDiscovery"},
        {"devicesCtrl", "rescanDevice"},
        {"devicesCtrl", "renameDevice"},
        {"devicesCtrl", "setDeviceDisabled"},
        {"devicesCtrl", "assignSite"},
        {"devicesCtrl", "removeDevice"},
        {"devicesCtrl", "renameChannel"},
        {"devicesCtrl", "setChannelDisabled"},
        {"devicesCtrl", "removeChannel"},
        {"devicesCtrl", "moveChannel"},
        {"devicesCtrl", "acknowledge"},
        {"devicesCtrl", "setMaintenance"},

        // --- Alarms (AlarmController, QML `alarmsCtrl`) ---------------------
        {"alarmsCtrl", "acknowledge"},
        {"alarmsCtrl", "escalate"},
        {"alarmsCtrl", "clearAlarm"},
        {"alarmsCtrl", "setDeviceMaintenance"},

        // --- Instant replay (InstantReplayController, QML `instant`, and the
        // overlay's `ir` property alias for the same object).
        {"instant", "replay"},
        {"instant", "returnToLive"},
        {"ir", "replay"},
        {"ir", "returnToLive"},

        // --- Playback transport (PlaybackController, reached in QML through
        // the `pb` / `ipb` property aliases). Discrete controls must route
        // through inc-30 commands. Only in-progress scrub frames are exempt;
        // QML commits the final seek through playback.seek / replay.seek.
        {"pb", "play"},
        {"pb", "pause"},
        {"pb", "setSpeed"},
        {"pb", "stepFrames"},
        {"pb", "seekFrac", Exemption::ContinuousTransport,
         "in-progress scrub frames; release commits playback.seek through the envelope"},
        {"ipb", "play"},
        {"ipb", "pause"},
        {"ipb", "setSpeed"},
        {"ipb", "seekFrac", Exemption::ContinuousTransport,
         "in-progress scrub frames; release commits replay.seek through the envelope"},
    };
}

namespace {

std::string trimmed(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// True when `pos` in `line` begins a real identifier (not the tail of a longer
// one), so "transport.pb.play(" matches object "pb" but "mypb.play(" does not.
bool boundaryBefore(const std::string& line, std::size_t pos) {
    if (pos == 0) return true;
    const char c = line[pos - 1];
    return !(std::isalnum(static_cast<unsigned char>(c)) || c == '_');
}

// A line that only *defines* the property alias (e.g. `property var pb: ...`)
// is not a call site; call detection already requires "obj.method(".
bool isCommentLine(const std::string& line) {
    const std::string t = trimmed(line);
    return t.rfind("//", 0) == 0 || t.rfind("*", 0) == 0;
}

} // namespace

CoverageReport CheckCoverage(
    const std::vector<std::pair<std::string, std::string>>& files,
    const std::vector<MutatorSpec>& declared) {
    CoverageReport r;
    r.filesScanned = static_cast<int>(files.size());
    r.declaredMutators = static_cast<int>(declared.size());

    // Accumulate exempt hits so each exempt mutator is reported once with a
    // site count (honest: "we saw N direct calls and why that is allowed").
    std::vector<CoverageNote> notes;

    for (const auto& [name, contents] : files) {
        std::istringstream is(contents);
        std::string line;
        int lineNo = 0;
        while (std::getline(is, line)) {
            ++lineNo;
            if (isCommentLine(line)) continue;   // prose, not a call
            for (const MutatorSpec& m : declared) {
                const std::string needle = m.object + "." + m.method + "(";
                std::size_t pos = line.find(needle);
                while (pos != std::string::npos) {
                    if (boundaryBefore(line, pos)) {
                        ++r.directCallSites;
                        if (m.exemption == Exemption::None) {
                            r.violations.push_back(
                                {name, lineNo, m.object, m.method, trimmed(line)});
                        } else {
                            bool merged = false;
                            for (CoverageNote& n : notes) {
                                if (n.object == m.object && n.method == m.method) {
                                    ++n.sites;
                                    merged = true;
                                    break;
                                }
                            }
                            if (!merged)
                                notes.push_back({m.object, m.method, m.exemption,
                                                 m.reason, 1});
                        }
                    }
                    pos = line.find(needle, pos + 1);
                }
            }
        }
    }
    r.notes = std::move(notes);
    return r;
}

std::string FormatCoverageReport(const CoverageReport& r) {
    std::ostringstream os;
    os << "command coverage (P1-13): " << r.filesScanned << " shipped QML file(s), "
       << r.declaredMutators << " declared mutator(s), " << r.directCallSites
       << " direct call site(s)\n";

    if (!r.notes.empty()) {
        os << "\ndeclared exemptions actually found (no silent skips):\n";
        for (const CoverageNote& n : r.notes)
            os << "  - " << n.object << "." << n.method << "  ["
               << ExemptionName(n.exemption) << " x" << n.sites << "]\n      "
               << n.reason << "\n";
    }

    if (r.violations.empty()) {
        os << "\nPASS: every state-changing UI action routes through the command "
              "envelope\n";
    } else {
        os << "\nFAIL: " << r.violations.size()
           << " UI action(s) bypass the command layer:\n";
        for (const CoverageViolation& v : r.violations)
            os << "  - " << v.file << ":" << v.line << "  " << v.object << "."
               << v.method << "()\n      " << v.text << "\n";
    }
    return os.str();
}

} // namespace vms::command
