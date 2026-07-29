#include "optimize/Optimizer.h"

#include <algorithm>
#include <cmath>

namespace vms::optimize {

namespace {
// A decrease applies immediately (degrade before exhaustion). A small increase
// (recovery) is suppressed so budget noise does not flap the plan.
double deadbanded(double prev, bool have, double cur, double band) {
    if (!have || prev <= 0.0) return cur;
    if (cur < prev) return cur;                       // tighten now (safety)
    if (cur - prev <= band * prev) return prev;       // suppress a small recovery
    return cur;
}
}  // namespace

OptimizerSession::OptimizerSession(double memSafetyFrac, double deadbandFrac)
    : memSafetyFrac_(memSafetyFrac), deadbandFrac_(deadbandFrac) {}

vms::CapacityProfile OptimizerSession::adjust(const vms::CapacityProfile& base,
                                              const SystemSample& s,
                                              OptimizationReport& report) {
    vms::CapacityProfile p = base;   // start from the static ceiling; never exceed it
    std::string clamped = "none";

    // Memory: never advertise more budget than a safe fraction of FREE RAM.
    if (s.valid && s.availRamMb > 0.0) {
        const double ramCeil = s.availRamMb * memSafetyFrac_;
        if (ramCeil < p.memoryBudgetMb) {
            p.memoryBudgetMb = ramCeil;
            clamped = "ram";
        }
    }

    // Decode: on a software (CPU-bound) machine, bound by CPU headroom. On a
    // hardware-decode machine, GPU decode does not scale with CPU load, so leave
    // the decode budget at its calibrated ceiling.
    if (s.valid && !base.hardwareDecode && s.systemCpuLoadPct >= 0.0) {
        const double freeFrac =
            std::clamp((100.0 - s.systemCpuLoadPct) / 100.0, 0.05, 1.0);
        const double cpuCeil = base.decodeBudget * freeFrac;
        if (cpuCeil < p.decodeBudget) {
            p.decodeBudget = cpuCeil;
            if (clamped == "none") clamped = "cpu";
        }
    }

    // Bandwidth: adopt the estimated link budget so admission respects it too.
    if (s.valid && s.estBandwidthKbps >= 0.0)
        p.bandwidthBudgetKbps = s.estBandwidthKbps;

    // Anti-flap deadband (recovery only).
    p.decodeBudget = deadbanded(prevDecode_, havePrev_, p.decodeBudget, deadbandFrac_);
    p.memoryBudgetMb = deadbanded(prevMemory_, havePrev_, p.memoryBudgetMb, deadbandFrac_);
    prevDecode_ = p.decodeBudget;
    prevMemory_ = p.memoryBudgetMb;
    havePrev_ = true;

    report.baseDecodeBudget = base.decodeBudget;
    report.baseMemoryBudgetMb = base.memoryBudgetMb;
    report.decodeBudget = p.decodeBudget;
    report.memoryBudgetMb = p.memoryBudgetMb;
    report.bandwidthBudgetKbps = p.bandwidthBudgetKbps;
    report.availRamMb = s.availRamMb;
    report.systemCpuLoadPct = s.systemCpuLoadPct;
    report.clampedBy = clamped;
    return p;
}

std::string OptimizerSession::bindingBudget(const vms::CapacityProfile& eff,
                                            const vms::GovernorResult& plan) {
    double best = -1.0;
    std::string which = "none";
    auto consider = [&](double used, double budget, const char* name) {
        if (budget <= 0.0) return;
        const double ratio = used / budget;
        if (ratio > best) { best = ratio; which = name; }
    };
    consider(plan.decodeUsed, eff.decodeBudget, "decode");
    consider(plan.memoryUsedMb, eff.memoryBudgetMb, "memory");
    consider(plan.bandwidthUsedKbps, eff.bandwidthBudgetKbps, "bandwidth");
    return which;
}

} // namespace vms::optimize
