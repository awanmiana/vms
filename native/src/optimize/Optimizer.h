#pragma once

// Optimizer — native increment 8. Scope: ../optimization-service-P12-03-proposal.md.
//
// Maps live system headroom (SystemSample) onto the governor's CapacityProfile so
// the wall degrades BEFORE the machine is exhausted, and recovers when headroom
// returns. Two rules keep it honest and stable:
//   - it NEVER raises a budget above the machine's static ceiling (the base
//     profile from the hardware probe / calibration);
//   - a decrease (tightening under pressure) applies immediately for safety, but
//     a small increase (recovery) is suppressed by a deadband so the budget does
//     not flap on sample noise.
// It also adopts an estimated link budget as the governor's bandwidth budget.
// It is PURE (no OS, no Qt) — the SystemHealthProbe does the sampling — so the
// whole feedback loop is unit-testable with a scripted fake probe.

#include <string>

#include "governor/Governor.h"
#include "optimize/SystemHealth.h"

namespace vms::optimize {

// Honest, read-only advice — never an automatic OS mutation.
struct OptimizationReport {
    double baseDecodeBudget = 0.0;      // the static ceilings, for reference
    double baseMemoryBudgetMb = 0.0;
    double decodeBudget = 0.0;          // the effective (adjusted) budgets
    double memoryBudgetMb = 0.0;
    double bandwidthBudgetKbps = 0.0;
    double availRamMb = -1.0;           // live sample echo
    double systemCpuLoadPct = -1.0;
    // Which budget the optimizer reduced from its static ceiling: ram / cpu / none.
    std::string clampedBy = "none";
    std::string note;
};

class OptimizerSession {
public:
    // memSafetyFrac: fraction of FREE RAM the memory budget may use.
    // deadbandFrac: minimum relative increase before a recovery is applied.
    explicit OptimizerSession(double memSafetyFrac = 0.6, double deadbandFrac = 0.10);

    // Adjust `base` for the live `sample`, filling `report`. Stateful for the
    // recovery deadband.
    vms::CapacityProfile adjust(const vms::CapacityProfile& base,
                                const SystemSample& sample,
                                OptimizationReport& report);

    // Name the budget nearest its cap in `plan` under the effective profile
    // (decode / memory / bandwidth / none) — the honest "what limited this plan".
    static std::string bindingBudget(const vms::CapacityProfile& effective,
                                     const vms::GovernorResult& plan);

private:
    double memSafetyFrac_;
    double deadbandFrac_;
    double prevDecode_ = 0.0;
    double prevMemory_ = 0.0;
    bool havePrev_ = false;
};

} // namespace vms::optimize
