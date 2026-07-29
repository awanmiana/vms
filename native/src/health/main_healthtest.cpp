// Device health self-check — native increment 13 (P2-13). No Qt, no network.
// Drives HealthMonitor with a deterministic clock and proves: honest Unknown
// until observed, separate dimensions deriving the overall state, the exception
// raise -> acknowledge -> auto-clear lifecycle, maintenance suppression, and the
// notification / needs-attention work-list. Registered as `health_selfcheck`.

#include "health/HealthMonitor.h"

#include <cstdint>
#include <iostream>
#include <string>

using namespace vms::health;

namespace {
int failures = 0;
void check(bool cond, const std::string& what) {
    std::cout << (cond ? "  ok  " : "  FAIL ") << what << "\n";
    if (!cond) ++failures;
}
}  // namespace

int main() {
    std::cout << "vms_healthtest — device health self-check\n";

    std::int64_t clockMs = 100000;
    NowMsFn clock = [&clockMs]() { return clockMs; };
    HealthMonitor h(clock);

    // Honest Unknown until observed.
    h.add("dev-1");
    check(h.state("dev-1") == State::Unknown, "new device is Unknown until observed");
    check(!h.shouldNotify("dev-1"), "Unknown device raises no alert");

    // Online, all good.
    h.reportReachable("dev-1", true);
    check(h.state("dev-1") == State::Online, "reachable + no subsystem fault -> Online");
    check(!h.shouldNotify("dev-1"), "healthy device has no exception");

    // Stream fails -> Degraded + exception raised.
    h.reportStream("dev-1", Stream::Failed);
    check(h.state("dev-1") == State::Degraded, "online device with a failed stream -> Degraded");
    {
        const DeviceHealth d = h.get("dev-1");
        check(d.exceptionActive && d.exceptionReason == "stream failed" &&
                  d.exceptionSinceMs == 100000,
              "exception raised with reason + first-seen time");
    }
    check(h.shouldNotify("dev-1"), "unacknowledged exception -> shouldNotify");
    check(h.needsAttention().size() == 1 && h.needsAttention()[0] == "dev-1",
          "device appears in the needs-attention work-list");

    // Acknowledge: silences the alert but the exception persists.
    h.acknowledge("dev-1");
    check(h.get("dev-1").exceptionActive && h.get("dev-1").exceptionAcknowledged,
          "acknowledge keeps the exception active");
    check(!h.shouldNotify("dev-1"), "acknowledged exception does not alert");
    check(h.needsAttention().empty(), "acknowledged device leaves the work-list");
    check(h.state("dev-1") == State::Degraded, "state is still honestly Degraded");

    // Recovery auto-clears the exception (and the acknowledgement).
    h.reportStream("dev-1", Stream::Ok);
    check(h.state("dev-1") == State::Online, "stream recovers -> Online");
    check(!h.get("dev-1").exceptionActive && !h.get("dev-1").exceptionAcknowledged,
          "recovery auto-clears the exception and its acknowledgement");

    // Offline raises a fresh exception at the new time.
    clockMs = 200000;
    h.reportReachable("dev-1", false);
    check(h.state("dev-1") == State::Offline, "unreachable -> Offline");
    check(h.get("dev-1").exceptionActive && h.get("dev-1").exceptionReason == "device offline" &&
              h.get("dev-1").exceptionSinceMs == 200000,
          "offline raises a fresh exception with a new first-seen time");

    // Maintenance window suppresses the alert but not the honest state.
    h.setMaintenance("dev-1", true);
    check(!h.shouldNotify("dev-1") && h.state("dev-1") == State::Offline,
          "maintenance window suppresses the alert (state still Offline)");
    h.setMaintenance("dev-1", false);
    check(h.shouldNotify("dev-1"), "leaving maintenance re-enables the alert");

    // Storage dimension is independent.
    h.reportReachable("dev-1", true);
    h.reportStorage("dev-1", Storage::Low);
    check(h.state("dev-1") == State::Degraded &&
              h.get("dev-1").exceptionReason == "storage low",
          "low storage on an online device -> Degraded (storage dimension)");
    h.reportStorage("dev-1", Storage::Ok);
    check(h.state("dev-1") == State::Online, "storage recovers -> Online");

    // Unsupported device + firmware inventory.
    h.add("dev-2");
    h.setSupported("dev-2", false);
    check(h.state("dev-2") == State::Unsupported, "unsupported device -> Unsupported");
    h.setFirmware("dev-2", "V5.4.5");
    check(h.get("dev-2").firmware == "V5.4.5", "firmware inventory recorded");

    if (failures == 0) {
        std::cout << "PASS: honest states, dimension independence, exception "
                     "lifecycle, maintenance suppression, and alerting verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}
