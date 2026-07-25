#include "governor/Governor.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <tuple>
#include <utility>

namespace {

double NonNegativeFinite(double value) {
    return std::isfinite(value) && value > 0.0 ? value : 0.0;
}

// Honest per-tile state from the request and the tier the governor assigned.
vms::TileState DeriveTileState(const vms::TileRequest& r, vms::Tier assigned) {
    if (assigned != vms::Tier::Paused)
        return assigned < r.desired ? vms::TileState::Degraded : vms::TileState::Live;
    // Not decoding: distinguish "off-screen by intent" from "wanted video but
    // capacity forced it off" so the UI can label the difference honestly.
    if (r.visible && r.desired != vms::Tier::Paused)
        return vms::TileState::PausedCapacity;
    return vms::TileState::PausedOffscreen;
}

} // namespace

namespace vms {

const char* TierName(Tier t) {
    switch (t) {
        case Tier::Main:  return "main";
        case Tier::Sub:   return "sub";
        case Tier::Thumb: return "thumb";
        case Tier::Paused: return "paused";
    }
    return "?";
}

const char* PriorityName(Priority p) {
    switch (p) {
        case Priority::High:   return "high";
        case Priority::Medium: return "medium";
        case Priority::Low:    return "low";
        case Priority::Idle:   return "idle";
    }
    return "?";
}

const char* TileStateName(TileState s) {
    switch (s) {
        case TileState::Live:            return "live";
        case TileState::Degraded:        return "degraded";
        case TileState::PausedOffscreen: return "paused(offscreen)";
        case TileState::PausedCapacity:  return "paused(capacity)";
    }
    return "?";
}

const TierCost& CostModel::forTier(Tier t) const {
    switch (t) {
        case Tier::Main:  return main;
        case Tier::Sub:   return sub;
        case Tier::Thumb: return thumb;
        case Tier::Paused: return paused;
    }
    return paused;
}

Governor::Governor(CapacityProfile profile, CostModel costs)
    : profile_(std::move(profile)), costs_(costs) {}

GovernorResult Governor::assign(const std::vector<TileRequest>& requests) const {
    const std::size_t n = requests.size();

    // 1. Seed each tile at its desired tier; an off-screen tile never decodes.
    std::vector<TileDecision> d(n);
    for (std::size_t i = 0; i < n; ++i) {
        const Tier t = requests[i].visible ? requests[i].desired : Tier::Paused;
        d[i].id = requests[i].id;
        d[i].tier = t;
        d[i].degraded = t < requests[i].desired;
    }

    auto recompute = [&](double& dec, double& mem) {
        dec = 0.0;
        mem = 0.0;
        for (const auto& x : d) {
            const TierCost& c = costs_.forTier(x.tier);
            dec += c.decode;
            mem += c.memoryMb;
        }
    };

    const double decCap = profile_.decodeBudget * profile_.highWatermark;
    const double memCap = profile_.memoryBudgetMb * profile_.highWatermark;

    double dec = 0.0, mem = 0.0;
    recompute(dec, mem);

    // 2. Degrade one step at a time until within both caps, choosing the least
    //    important tile to downgrade. Ordering (pick the minimum): non-focused
    //    before focused; lower priority before higher; higher tier before lower
    //    (drop a Main before a Sub); larger id first (deterministic tie-break).
    auto candidateScore = [&](std::size_t i) {
        const TileRequest& r = requests[i];
        return std::make_tuple(r.focused ? 1 : 0,
                               static_cast<int>(r.priority),
                               -static_cast<int>(d[i].tier),
                               -r.id);
    };

    while (dec > decCap || mem > memCap) {
        long best = -1;
        for (std::size_t i = 0; i < n; ++i) {
            if (d[i].tier == Tier::Paused) continue; // nothing left to shed here
            if (best < 0 || candidateScore(i) < candidateScore(static_cast<std::size_t>(best)))
                best = static_cast<long>(i);
        }
        if (best < 0) break; // everything already paused

        const std::size_t b = static_cast<std::size_t>(best);
        d[b].tier = static_cast<Tier>(static_cast<int>(d[b].tier) - 1);
        d[b].degraded = d[b].tier < requests[b].desired;
        recompute(dec, mem);
    }

    GovernorResult res;
    res.decodeUsed = dec;
    res.memoryUsedMb = mem;
    res.decoding = 0;
    res.overflow = false;
    for (std::size_t i = 0; i < n; ++i) {
        d[i].state = DeriveTileState(requests[i], d[i].tier);
        if (d[i].tier != Tier::Paused) ++res.decoding;
        if (requests[i].visible && requests[i].desired != Tier::Paused &&
            d[i].tier == Tier::Paused)
            res.overflow = true;
    }
    res.tiles = std::move(d);  // per-tile state filled in
    return res;
}

std::vector<TileTransition> DiffPlans(const GovernorResult& before,
                                     const GovernorResult& after) {
    auto tierOf = [](const GovernorResult& r, int id, bool& found) {
        for (const auto& d : r.tiles)
            if (d.id == id) { found = true; return d.tier; }
        found = false;
        return Tier::Paused;
    };

    // Union of ids: those in `after` first (their current order), then any tile
    // that only existed in `before` (it is departing to Paused).
    std::vector<int> ids;
    ids.reserve(after.tiles.size() + before.tiles.size());
    auto pushUnique = [&](int id) {
        for (int existing : ids)
            if (existing == id) return;
        ids.push_back(id);
    };
    for (const auto& d : after.tiles) pushUnique(d.id);
    for (const auto& d : before.tiles) pushUnique(d.id);

    std::vector<TileTransition> out;
    for (int id : ids) {
        bool inBefore = false, inAfter = false;
        const Tier from = tierOf(before, id, inBefore);
        const Tier to = tierOf(after, id, inAfter);
        if (from == to) continue; // includes absent-from-both and unchanged
        TileTransition t;
        t.id = id;
        t.from = from;
        t.to = to;
        out.push_back(t);
    }

    // Releases before acquires so a live apply never over-subscribes mid-sequence.
    std::stable_sort(out.begin(), out.end(),
                     [](const TileTransition& a, const TileTransition& b) {
                         if (a.release() != b.release()) return a.release();
                         return a.id < b.id;
                     });
    return out;
}

GovernorSession::GovernorSession(CapacityProfile profile, CostModel costs)
    : profile_(std::move(profile)), costs_(costs) {}

GovernorResult GovernorSession::update(const std::vector<TileRequest>& requests) {
    const GovernorResult highPlan = Governor(profile_, costs_).assign(requests);

    auto remember = [&](const GovernorResult& result) {
        previousRequests_ = requests;
        previousResult_ = result;
        initialized_ = true;
        return result;
    };

    if (!initialized_) return remember(highPlan);

    auto requestIndex = [](const std::vector<TileRequest>& items, int id) {
        for (std::size_t i = 0; i < items.size(); ++i)
            if (items[i].id == id) return static_cast<long>(i);
        return -1L;
    };
    auto decisionIndex = [](const std::vector<TileDecision>& items, int id) {
        for (std::size_t i = 0; i < items.size(); ++i)
            if (items[i].id == id) return static_cast<long>(i);
        return -1L;
    };
    auto hasDuplicateIds = [](const std::vector<TileRequest>& items) {
        for (std::size_t i = 0; i < items.size(); ++i)
            for (std::size_t j = i + 1; j < items.size(); ++j)
                if (items[i].id == items[j].id) return true;
        return false;
    };

    // Duplicate IDs make historical matching ambiguous. Keep the stateless
    // governor's deterministic and budget-safe behavior for that malformed input.
    if (hasDuplicateIds(requests) || hasDuplicateIds(previousRequests_))
        return remember(highPlan);

    bool loadIncreased = false;
    bool loadDecreased = false;
    bool priorityChanged = false;
    for (const auto& request : requests) {
        const long oldIndex = requestIndex(previousRequests_, request.id);
        const Tier nowWanted = request.visible ? request.desired : Tier::Paused;
        if (oldIndex < 0) {
            if (nowWanted != Tier::Paused) loadIncreased = true;
            continue;
        }

        const TileRequest& old = previousRequests_[static_cast<std::size_t>(oldIndex)];
        const Tier oldWanted = old.visible ? old.desired : Tier::Paused;
        if (nowWanted > oldWanted) loadIncreased = true;
        if (nowWanted < oldWanted) loadDecreased = true;
        if (request.focused != old.focused || request.priority != old.priority)
            priorityChanged = true;
    }
    for (const auto& old : previousRequests_) {
        if (requestIndex(requests, old.id) < 0 &&
            old.visible && old.desired != Tier::Paused)
            loadDecreased = true;
    }

    // New demand and importance changes must re-run admission so a newly
    // focused/high-priority tile can displace a less important one immediately.
    if (loadIncreased || priorityChanged) return remember(highPlan);

    auto resultFromTiers = [&](const std::vector<Tier>& tiers) {
        GovernorResult result;
        result.tiles.reserve(requests.size());
        for (std::size_t i = 0; i < requests.size(); ++i) {
            TileDecision decision;
            decision.id = requests[i].id;
            decision.tier = tiers[i];
            decision.degraded = tiers[i] < requests[i].desired;
            decision.state = DeriveTileState(requests[i], tiers[i]);
            result.tiles.push_back(decision);

            const TierCost& cost = costs_.forTier(tiers[i]);
            result.decodeUsed += cost.decode;
            result.memoryUsedMb += cost.memoryMb;
            if (tiers[i] != Tier::Paused) ++result.decoding;
            if (requests[i].visible && requests[i].desired != Tier::Paused &&
                tiers[i] == Tier::Paused)
                result.overflow = true;
        }
        return result;
    };

    // Rebuild the previous assignment in the caller's current order, clamping
    // immediately to a lowered desired tier or Paused visibility.
    std::vector<Tier> current(requests.size(), Tier::Paused);
    for (std::size_t i = 0; i < requests.size(); ++i) {
        if (!requests[i].visible) continue;
        const long oldDecision = decisionIndex(previousResult_.tiles, requests[i].id);
        if (oldDecision < 0) continue;
        const Tier oldTier =
            previousResult_.tiles[static_cast<std::size_t>(oldDecision)].tier;
        current[i] = std::min(oldTier, requests[i].desired);
    }

    // Identical demand returns the identical assignment: no periodic re-plan,
    // no tier churn, and no dependence on vector order.
    if (!loadDecreased) return remember(resultFromTiers(current));

    GovernorResult currentResult = resultFromTiers(current);
    const double highDecodeCap = profile_.decodeBudget * profile_.highWatermark;
    const double highMemoryCap = profile_.memoryBudgetMb * profile_.highWatermark;
    if (currentResult.decodeUsed > highDecodeCap + 1e-9 ||
        currentResult.memoryUsedMb > highMemoryCap + 1e-9)
        return remember(highPlan);

    const double lowDecodeCap = profile_.decodeBudget * profile_.lowWatermark;
    const double lowMemoryCap = profile_.memoryBudgetMb * profile_.lowWatermark;

    // Recover one tier at a time, most-important first, but only when the
    // upgraded plan remains below BOTH low watermarks.
    for (;;) {
        long best = -1;
        for (std::size_t i = 0; i < requests.size(); ++i) {
            if (!requests[i].visible || current[i] >= requests[i].desired) continue;

            const Tier next =
                static_cast<Tier>(static_cast<int>(current[i]) + 1);
            const TierCost& before = costs_.forTier(current[i]);
            const TierCost& after = costs_.forTier(next);
            if (currentResult.decodeUsed + after.decode - before.decode >
                    lowDecodeCap + 1e-9 ||
                currentResult.memoryUsedMb + after.memoryMb - before.memoryMb >
                    lowMemoryCap + 1e-9)
                continue;

            auto score = [&](std::size_t index) {
                const TileRequest& r = requests[index];
                return std::make_tuple(r.focused ? 0 : 1,
                                       -static_cast<int>(r.priority),
                                       -static_cast<int>(r.desired),
                                       r.id);
            };
            if (best < 0 || score(i) < score(static_cast<std::size_t>(best)))
                best = static_cast<long>(i);
        }
        if (best < 0) break;

        const std::size_t index = static_cast<std::size_t>(best);
        const TierCost before = costs_.forTier(current[index]);
        current[index] =
            static_cast<Tier>(static_cast<int>(current[index]) + 1);
        const TierCost after = costs_.forTier(current[index]);
        currentResult.decodeUsed += after.decode - before.decode;
        currentResult.memoryUsedMb += after.memoryMb - before.memoryMb;
    }

    return remember(resultFromTiers(current));
}

void GovernorSession::reset() {
    previousRequests_.clear();
    previousResult_ = GovernorResult{};
    initialized_ = false;
}

CapacityProfile DevBoxProfile() {
    CapacityProfile p;
    p.label = "dev box (RTX 3050, hardware H.265, d3d12h265dec)";
    // Corrected I420 ramp (MEASUREMENTS.md 2026-07-25): 64 concurrent 1080p main
    // decoders verified running at ~1722 agg fps / 2.75GB RSS; throughput ceiling
    // ~70. Seed at the verified 64 (conservative vs the 70 ceiling).
    p.decodeBudget = 64.0;
    p.memoryBudgetMb = 6000.0;  // generous safety net; 64 main measured only 2.75GB (throughput-bound)
    p.hardwareDecode = true;
    return p;
}

CapacityProfile LowEndProfile() {
    CapacityProfile p;
    // PLACEHOLDER until the real i5 4th-gen ramp is run (see MEASUREMENTS.md
    // "STILL NEEDED"). Software decode is CPU-bound, so the budget is small.
    p.label = "low-end i5 4th-gen no-GPU (software, PLACEHOLDER)";
    p.decodeBudget = 3.0;
    p.memoryBudgetMb = 1500.0;
    p.hardwareDecode = false;
    return p;
}

CapacityProfile MakeCapacityProfile(const HardwareInputs& hw, const std::string& label) {
    CapacityProfile p;
    p.label = label.empty() ? "auto-probed machine" : label;
    p.hardwareDecode = hw.hasHardwareDecode;

    const double ramMb = NonNegativeFinite(hw.totalRamMb);
    const double vramMb = NonNegativeFinite(hw.videoMemoryMb);
    const double cores = static_cast<double>(std::max(1u, hw.logicalCores));

    // Static specifications cannot reveal a decoder's true throughput. Seed
    // below (and never above) the measured dev-box ceiling; a recorded ramp can
    // later replace this heuristic with a calibrated profile. CPU count is only
    // a coarse machine-class proxy on the hardware path, not a claim that GPU
    // decode scales with CPU cores.
    if (hw.hasHardwareDecode) {
        // A modern GPU decode engine sustains far more than the old core-count
        // seed implied: the corrected d3d12h265dec ramp measured ~64 sustainable
        // 1080p main streams on a mid-range RTX 3050 (MEASUREMENTS.md). Throughput
        // is not derivable from specs, so still seed BELOW the measured ceiling
        // (degrade early, never crash) and let a per-GPU ramp refine it; VRAM is
        // the best spec-level proxy for GPU class. CPU cores are not used here —
        // GPU decode does not scale with them.
        double decodeBudget = 12.0;               // conservative floor for any HW-decode GPU
        if (vramMb >= 6144.0)      decodeBudget = 40.0;  // strong dGPU
        else if (vramMb >= 3072.0) decodeBudget = 28.0;  // mid dGPU (RTX 3050 class)
        else if (vramMb >= 1024.0) decodeBudget = 20.0;  // entry dGPU
        // integrated/shared graphics report ~0 dedicated VRAM -> keep the floor.
        p.decodeBudget = std::min(decodeBudget, DevBoxProfile().decodeBudget);
    } else {
        // Software decode is CPU-bound. Keep the uncalibrated seed small while
        // still allowing one focused Main tile below the 90% high watermark.
        p.decodeBudget = std::clamp(cores * 0.5, 1.25, 4.0);
    }

    // This budget covers modeled per-stream resident memory, not the process
    // baseline. Use at most 20% of system RAM and cap the uncalibrated seed at
    // the already-proven 6 GB dev-box profile. Dedicated VRAM is an additional
    // upper bound when known; integrated/shared memory is passed as zero.
    const double systemBudgetMb =
        std::clamp(ramMb > 0.0 ? ramMb * 0.20 : 1024.0, 512.0, 6000.0);
    if (hw.hasHardwareDecode && vramMb >= 512.0) {
        p.memoryBudgetMb = std::min(systemBudgetMb, std::max(512.0, vramMb * 0.75));
    } else {
        p.memoryBudgetMb = systemBudgetMb;
    }

    return p;
}

} // namespace vms
