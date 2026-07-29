// Optimization service self-check — native increment 8. No Qt, no display, no
// camera. Drives the Optimizer with a scripted FakeSystemHealthProbe and the real
// governor to prove the acceptance criteria (proposal §7): proactive degrade
// before exhaustion, recovery without flapping, bandwidth admission, and an
// honest report naming the binding budget. Also smoke-checks the real Windows
// probe. Registered with CTest as `optimize_selfcheck`.
// Scope: ../optimization-service-P12-03-proposal.md.

#include "governor/Governor.h"
#include "optimize/Optimizer.h"
#include "optimize/SystemHealth.h"

#include <iostream>
#include <string>
#include <vector>

using namespace vms;
using namespace vms::optimize;

namespace {

int failures = 0;
void check(bool cond, const std::string& what) {
    std::cout << (cond ? "  ok  " : "  FAIL ") << what << "\n";
    if (!cond) ++failures;
}

int countTier(const GovernorResult& r, Tier t) {
    int c = 0;
    for (const auto& d : r.tiles) if (d.tier == t) ++c;
    return c;
}

std::vector<TileRequest> mainRequests(int n) {
    std::vector<TileRequest> v;
    for (int i = 0; i < n; ++i) {
        TileRequest r;
        r.id = i; r.desired = Tier::Main; r.priority = Priority::Medium;
        r.focused = (i == 0); r.visible = true;
        v.push_back(r);
    }
    return v;
}

SystemSample sampleWith(double totalMb, double availMb, double cpu, double bw) {
    SystemSample s;
    s.valid = true; s.totalRamMb = totalMb; s.availRamMb = availMb;
    s.logicalCores = 8; s.systemCpuLoadPct = cpu; s.estBandwidthKbps = bw;
    return s;
}

}  // namespace

int main() {
    std::cout << "vms_opttest — optimization service self-check\n";

    CapacityProfile swBase;   // software-decode machine
    swBase.decodeBudget = 10.0;
    swBase.memoryBudgetMb = 1000.0;
    swBase.hardwareDecode = false;
    swBase.label = "test-sw";

    // ---- 1) Ample headroom: effective == base, nothing degraded ----
    {
        OptimizerSession opt(0.5, 0.10);
        OptimizationReport rep;
        const CapacityProfile eff =
            opt.adjust(swBase, sampleWith(16000, 16000, 0.0, -1.0), rep);
        const GovernorResult plan = Governor(eff).assign(mainRequests(8));
        check(rep.clampedBy == "none", "ample headroom: nothing clamped");
        check(countTier(plan, Tier::Main) == 8, "ample headroom: all 8 tiles stay Main");
    }

    // ---- 2) RAM pressure: proactive degrade before exhaustion ----
    {
        OptimizerSession opt(0.5, 0.10);
        OptimizationReport rep;
        const CapacityProfile eff =
            opt.adjust(swBase, sampleWith(16000, 400, 0.0, -1.0), rep);   // ramCeil=200
        const GovernorResult plan = Governor(eff).assign(mainRequests(8));
        check(rep.clampedBy == "ram" && eff.memoryBudgetMb == 200.0,
              "RAM pressure clamps the memory budget to free-RAM headroom");
        check(countTier(plan, Tier::Main) < 8,
              "RAM pressure degrades tiles below the static-ceiling plan");
        check(plan.memoryUsedMb <= eff.memoryBudgetMb * eff.highWatermark + 1e-9,
              "the emitted plan stays under the lowered budget (degrade BEFORE exhaustion)");
        check(OptimizerSession::bindingBudget(eff, plan) == "memory",
              "the report attributes the degradation to the memory budget");
    }

    // ---- 3) Recovery without flapping (deadband) ----
    {
        OptimizerSession opt(0.5, 0.10);
        OptimizationReport rep;
        // Establish the budget at 500 (avail 1000 -> ramCeil 500).
        CapacityProfile eff = opt.adjust(swBase, sampleWith(16000, 1000, 0.0, -1.0), rep);
        check(eff.memoryBudgetMb == 500.0, "deadband: initial budget follows headroom (500)");
        // A small recovery (avail 1080 -> ramCeil 540, +8% within the 10% band) is suppressed.
        eff = opt.adjust(swBase, sampleWith(16000, 1080, 0.0, -1.0), rep);
        check(eff.memoryBudgetMb == 500.0,
              "deadband: a small recovery is suppressed (no flap)");
        // A decrease applies immediately (safety), no deadband.
        eff = opt.adjust(swBase, sampleWith(16000, 800, 0.0, -1.0), rep);
        check(eff.memoryBudgetMb == 400.0,
              "a decrease applies immediately (degrade is never deadbanded)");
        // A large recovery beyond the band is applied, up to (never above) the ceiling.
        eff = opt.adjust(swBase, sampleWith(16000, 16000, 0.0, -1.0), rep);
        check(eff.memoryBudgetMb == 1000.0,
              "a large recovery restores the budget up to the static ceiling");
    }

    // ---- 4) Bandwidth admission ----
    {
        CapacityProfile hwBase;   // hardware decode, so only bandwidth binds
        hwBase.decodeBudget = 10.0;
        hwBase.memoryBudgetMb = 4000.0;
        hwBase.hardwareDecode = true;
        hwBase.label = "test-hw";

        OptimizerSession opt(0.6, 0.10);
        OptimizationReport rep;
        const CapacityProfile eff =
            opt.adjust(hwBase, sampleWith(16000, 16000, 50.0, 3000.0), rep);
        check(eff.bandwidthBudgetKbps == 3000.0,
              "bandwidth: the estimated link budget becomes the admission budget");
        const GovernorResult plan = Governor(eff).assign(mainRequests(4));
        check(countTier(plan, Tier::Main) < 4,
              "bandwidth: a tight link degrades tiles that decode+RAM alone would allow");
        check(plan.bandwidthUsedKbps <= eff.bandwidthBudgetKbps * eff.highWatermark + 1e-9,
              "bandwidth: the emitted plan fits under the link budget");
        check(OptimizerSession::bindingBudget(eff, plan) == "bandwidth",
              "the report attributes the degradation to the bandwidth budget");
    }

    // ---- 5) Two-budget behavior unchanged when bandwidth is unmodeled ----
    {
        OptimizerSession opt(0.6, 0.10);
        OptimizationReport rep;
        const CapacityProfile eff =
            opt.adjust(swBase, sampleWith(16000, 16000, 0.0, -1.0), rep);
        check(eff.bandwidthBudgetKbps == 0.0,
              "no link estimate leaves bandwidth unmodeled (two-budget behavior intact)");
    }

#ifdef _WIN32
    // ---- 6) The real Windows probe returns sane live numbers ----
    {
        WindowsSystemHealthProbe probe;
        probe.sample();                       // prime the CPU delta
        const SystemSample s = probe.sample();
        check(s.valid && s.availRamMb > 0.0 && s.logicalCores > 0,
              "Windows probe reports live RAM + cores");
        std::cout << "  (live: " << static_cast<long>(s.availRamMb) << "/"
                  << static_cast<long>(s.totalRamMb) << " MB free, "
                  << s.logicalCores << " cores, cpu "
                  << (s.systemCpuLoadPct < 0 ? -1 : static_cast<int>(s.systemCpuLoadPct))
                  << "%)\n";
    }
#endif

    if (failures == 0) {
        std::cout << "PASS: proactive degrade, recovery without flapping, bandwidth "
                     "admission, and honest binding-budget report all verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}
