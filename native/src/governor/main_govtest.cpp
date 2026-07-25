// Self-check harness for the P3-03 decode governor (native increment 4b).
//
// Pure logic, no GStreamer/OS deps: builds capacity profiles from the measured
// ceilings and asserts the acceptance criteria from governor-P3-03-proposal.md
// section 10 - above all #1: requesting 64 main tiles must CAP (fit within
// budget) instead of crashing. Prints each scenario, then PASS/FAIL. Exit code
// is non-zero if any check fails, so it can gate an automated build.

#include "governor/Governor.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace vms;

namespace {

int g_failures = 0;

void check(bool cond, const std::string& what) {
    std::cout << (cond ? "  [PASS] " : "  [FAIL] ") << what << "\n";
    if (!cond) ++g_failures;
}

struct TierTally { int main = 0, sub = 0, thumb = 0, paused = 0; };

TierTally tally(const GovernorResult& r) {
    TierTally t;
    for (const auto& d : r.tiles) {
        switch (d.tier) {
            case Tier::Main:  ++t.main; break;
            case Tier::Sub:   ++t.sub; break;
            case Tier::Thumb: ++t.thumb; break;
            case Tier::Paused: ++t.paused; break;
        }
    }
    return t;
}

void report(const std::string& name, const CapacityProfile& p, const GovernorResult& r) {
    const TierTally t = tally(r);
    std::cout << "\n== " << name << " ==\n"
              << "  profile: " << p.label << "  (decodeBudget=" << p.decodeBudget
              << " main-eq, memBudget=" << p.memoryBudgetMb << "MB)\n"
              << "  result : main=" << t.main << " sub=" << t.sub << " thumb=" << t.thumb
              << " paused=" << t.paused << "\n"
              << "  usage  : decode=" << r.decodeUsed << " main-eq  memory="
              << r.memoryUsedMb << "MB  decoding=" << r.decoding
              << "  overflow=" << (r.overflow ? "yes" : "no") << "\n";
}

// N tiles all wanting Main; tile 0 is the focused one.
std::vector<TileRequest> mainGrid(int n) {
    std::vector<TileRequest> v;
    v.reserve(n);
    for (int i = 0; i < n; ++i) {
        TileRequest r;
        r.id = i;
        r.desired = Tier::Main;
        r.priority = (i == 0) ? Priority::High : Priority::Medium;
        r.focused = (i == 0);
        r.visible = true;
        v.push_back(r);
    }
    return v;
}

Tier tierOf(const GovernorResult& r, int id) {
    for (const auto& d : r.tiles) if (d.id == id) return d.tier;
    return Tier::Paused;
}

TileState stateOf(const GovernorResult& r, int id) {
    for (const auto& d : r.tiles) if (d.id == id) return d.state;
    return TileState::PausedOffscreen;
}

bool near(double actual, double expected, double tolerance = 1e-6) {
    return std::abs(actual - expected) <= tolerance;
}

bool sameTiers(const GovernorResult& a, const GovernorResult& b) {
    if (a.tiles.size() != b.tiles.size()) return false;
    for (const auto& decision : a.tiles)
        if (tierOf(b, decision.id) != decision.tier) return false;
    return true;
}

} // namespace

int main() {
    std::cout << "P3-03 decode governor self-check\n"
              << "================================\n";

    // Scenario 1 (headline acceptance #1): 64 main tiles on the dev box.
    {
        const CapacityProfile p = DevBoxProfile();
        Governor gov(p);
        const auto req = mainGrid(64);
        const auto res = gov.assign(req);
        report("dev box: 64 main tiles (the crash case)", p, res);

        check(res.decodeUsed <= p.decodeBudget * p.highWatermark + 1e-6,
              "decode usage stays within budget (no over-subscription / crash)");
        check(res.memoryUsedMb <= p.memoryBudgetMb * p.highWatermark + 1e-6,
              "memory usage stays within the modeled budget");
        check(tierOf(res, 0) == Tier::Main,
              "focused high-priority tile keeps main tier under pressure");
        check(res.decoding == 64 && !res.overflow,
              "all 64 tiles still show video (degraded to cheaper tiers, none dropped)");
    }

    // Scenario 2: fits comfortably -> no degrade. 8 sub tiles on the dev box.
    {
        const CapacityProfile p = DevBoxProfile();
        Governor gov(p);
        std::vector<TileRequest> req;
        for (int i = 0; i < 8; ++i) { TileRequest r; r.id = i; r.desired = Tier::Sub; req.push_back(r); }
        const auto res = gov.assign(req);
        report("dev box: 8 sub tiles (well within budget)", p, res);
        const TierTally t = tally(res);
        check(t.sub == 8 && t.main == 0 && t.paused == 0,
              "within budget: every tile keeps its desired tier, nothing degraded");
    }

    // Scenario 3: off-screen tiles never decode.
    {
        const CapacityProfile p = DevBoxProfile();
        Governor gov(p);
        std::vector<TileRequest> req;
        for (int i = 0; i < 6; ++i) {
            TileRequest r; r.id = i; r.desired = Tier::Main; r.visible = (i < 2); req.push_back(r);
        }
        const auto res = gov.assign(req);
        report("dev box: 2 visible + 4 off-screen", p, res);
        check(res.decoding == 2, "only the 2 visible tiles decode; off-screen tiles are paused");
    }

    // Scenario 4: low-end (software) machine caps hard. 16 main tiles.
    {
        const CapacityProfile p = LowEndProfile();
        Governor gov(p);
        const auto req = mainGrid(16);
        const auto res = gov.assign(req);
        report("low-end (PLACEHOLDER): 16 main tiles", p, res);
        check(res.decodeUsed <= p.decodeBudget * p.highWatermark + 1e-6,
              "low-end: decode usage capped to the (small) software budget");
        check(tierOf(res, 0) == Tier::Main,
              "low-end: focused tile still protected at main tier");
    }

    // Scenario 5: live-probe inputs matching this dev box seed the measured,
    // bounded profile instead of relying on a hard-coded machine name.
    {
        HardwareInputs hw;
        hw.hasHardwareDecode = true;
        hw.totalRamMb = 64.0 * 1024.0;
        hw.videoMemoryMb = 8.0 * 1024.0;
        hw.logicalCores = 16;
        const CapacityProfile p = MakeCapacityProfile(hw, "synthetic probed dev box");
        const auto res = Governor(p).assign(mainGrid(64));
        report("auto profile: dev-box hardware inputs", p, res);
        check(p.hardwareDecode, "auto profile records hardware decode for the working codec");
        check(near(p.decodeBudget, 40.0),
              "auto profile seeds a strong-dGPU (>=6GB VRAM) budget below the measured 64 ceiling");
        check(near(p.memoryBudgetMb, 6000.0),
              "auto profile caps modeled stream memory at the proven 6GB seed");
        check(res.decodeUsed <= p.decodeBudget * p.highWatermark + 1e-6 &&
              res.memoryUsedMb <= p.memoryBudgetMb * p.highWatermark + 1e-6,
              "auto-profile 64-tile plan stays inside both budgets");
        check(tierOf(res, 0) == Tier::Main,
              "auto-profile plan protects the focused tile");
    }

    // Scenario 6: an uncalibrated 4-core/8GB software machine starts small.
    {
        HardwareInputs hw;
        hw.hasHardwareDecode = false;
        hw.totalRamMb = 8.0 * 1024.0;
        hw.logicalCores = 4;
        const CapacityProfile p = MakeCapacityProfile(hw, "synthetic low-end laptop");
        const auto res = Governor(p).assign(mainGrid(16));
        report("auto profile: 4-core/8GB software inputs", p, res);
        check(!p.hardwareDecode && near(p.decodeBudget, 2.0),
              "software profile seeds a conservative CPU-bound decode budget");
        check(p.memoryBudgetMb <= hw.totalRamMb * 0.20 + 1e-6,
              "software profile reserves at least 80% of system RAM outside stream costs");
        check(tierOf(res, 0) == Tier::Main,
              "software profile still admits one focused Main tile");
        check(res.decodeUsed <= p.decodeBudget * p.highWatermark + 1e-6 &&
              res.memoryUsedMb <= p.memoryBudgetMb * p.highWatermark + 1e-6,
              "software plan remains inside both conservative budgets");
    }

    // Scenario 7: incomplete/invalid probe data has a safe usable fallback.
    {
        HardwareInputs hw;
        const CapacityProfile p = MakeCapacityProfile(hw, "");
        const auto res = Governor(p).assign(mainGrid(1));
        report("auto profile: unknown hardware inputs", p, res);
        check(p.label == "auto-probed machine",
              "empty probe label receives a deterministic fallback");
        check(p.decodeBudget * p.highWatermark >= 1.0 &&
              p.memoryBudgetMb * p.highWatermark >= 200.0,
              "unknown profile can admit one focused Main tile");
        check(tierOf(res, 0) == Tier::Main,
              "unknown profile keeps the single requested tile at Main");
    }

    // Scenario 8: a stateful working set recovers only below the low watermark,
    // stays stable at steady load, and re-admits new/focused demand immediately.
    {
        const CapacityProfile p = DevBoxProfile();
        GovernorSession session(p);
        const auto fullRequests = mainGrid(64);
        const auto full = session.update(fullRequests);
        const auto steady = session.update(fullRequests);
        check(sameTiers(full, steady),
              "stateful session returns identical tiers for identical steady load");

        const auto smallRequests = mainGrid(8);
        const auto recovered = session.update(smallRequests);
        const TierTally recoveredTally = tally(recovered);
        check(recoveredTally.main == 8,
              "closing tiles recovers all remaining quality below the low watermark");

        const auto expanded = session.update(fullRequests);
        check(sameTiers(expanded, Governor(p).assign(fullRequests)),
              "new demand is immediately re-admitted against the high watermark");

        auto refocusedRequests = fullRequests;
        refocusedRequests[0].focused = false;
        refocusedRequests[0].priority = Priority::Medium;
        refocusedRequests[63].focused = true;
        refocusedRequests[63].priority = Priority::High;
        const auto refocused = session.update(refocusedRequests);
        check(tierOf(refocused, 63) == Tier::Main,
              "focus change immediately protects the newly focused tile");
        check(refocused.decodeUsed <= p.decodeBudget * p.highWatermark + 1e-6 &&
              refocused.memoryUsedMb <= p.memoryBudgetMb * p.highWatermark + 1e-6,
              "dynamic focus re-plan remains within both high-water budgets");
    }

    // Scenario 9: removing a degraded tile can leave the retained plan between
    // low and high watermarks; hysteresis holds it instead of churning tiers.
    {
        CapacityProfile p;
        p.label = "hysteresis boundary";
        p.decodeBudget = 5.0;      // high=4.5, low=3.75
        p.memoryBudgetMb = 10000.0;
        GovernorSession session(p);
        const auto five = session.update(mainGrid(5));
        const TierTally fiveTally = tally(five);
        check(fiveTally.main == 4 && fiveTally.sub == 1,
              "boundary setup fills capacity below the high watermark");

        const auto four = session.update(mainGrid(4));
        check(tally(four).main == 4 && near(four.decodeUsed, 4.0),
              "plan between low and high watermarks is retained after load drops");
        const auto fourAgain = session.update(mainGrid(4));
        check(sameTiers(four, fourAgain),
              "hysteresis boundary remains stable across repeated updates");
    }

    // Scenario 10: DiffPlans reports only changed tiles and orders every release
    // (downgrade/pause) before every acquire (upgrade/resume) so a live apply
    // never transiently over-subscribes the machine.
    {
        GovernorResult before;
        before.tiles = {{0, Tier::Main, false}, {1, Tier::Sub, false},
                        {2, Tier::Sub, false},  {3, Tier::Paused, false}};
        GovernorResult after;
        after.tiles = {{0, Tier::Sub, true},   // 0 downgraded (release)
                       {1, Tier::Sub, false},  // 1 unchanged -> omitted
                       {2, Tier::Main, false}, // 2 upgraded (acquire)
                       {3, Tier::Thumb, false}}; // 3 resumed (acquire)
        const auto diff = DiffPlans(before, after);
        std::cout << "\n== DiffPlans: release-before-acquire ordering ==\n";
        for (const auto& t : diff)
            std::cout << "  tile " << t.id << ": " << TierName(t.from) << " -> "
                      << TierName(t.to) << (t.release() ? "  (release)" : "  (acquire)")
                      << "\n";
        check(diff.size() == 3, "only the three changed tiles appear (unchanged tile omitted)");
        bool seenAcquire = false, ordered = true;
        for (const auto& t : diff) {
            if (t.acquire()) seenAcquire = true;
            if (t.release() && seenAcquire) ordered = false;
        }
        check(ordered, "all releases are ordered before all acquires");
        check(!diff.empty() && diff.front().id == 0 && diff.front().release(),
              "the sole release (tile 0 main->sub) is applied first");
        check(diff.back().id == 3 && diff.back().startsDecode(),
              "the resumed tile (paused->thumb) is ordered last among the acquires");
    }

    // Scenario 11: a runtime focus sweep across a full 64-main grid keeps every
    // step inside budget, always protects the focused tile, never pauses a tile
    // on the dev box, produces bounded churn, and does not flap on a repeat.
    {
        const CapacityProfile p = DevBoxProfile();
        GovernorSession session(p);
        auto grid = mainGrid(64);
        GovernorResult prev = session.update(grid);
        std::cout << "\n== runtime focus sweep (dev box, 64 main tiles) ==\n";

        const int focusStops[] = {1, 5, 31, 63, 0};
        bool allInBudget = true, focusProtected = true, noPause = true, bounded = true;
        for (int stop : focusStops) {
            for (auto& r : grid) { r.focused = false; r.priority = Priority::Medium; }
            grid[stop].focused = true;
            grid[stop].priority = Priority::High;

            const GovernorResult cur = session.update(grid);
            const auto moves = DiffPlans(prev, cur);
            std::cout << "  focus -> tile " << stop << ": " << moves.size()
                      << " tile transition(s), decode=" << cur.decodeUsed
                      << " mem=" << cur.memoryUsedMb << "MB\n";

            if (cur.decodeUsed > p.decodeBudget * p.highWatermark + 1e-6 ||
                cur.memoryUsedMb > p.memoryBudgetMb * p.highWatermark + 1e-6)
                allInBudget = false;
            if (tierOf(cur, stop) != Tier::Main) focusProtected = false;
            if (cur.decoding != 64 || cur.overflow) noPause = false;
            // A single focus change must not re-tier the whole grid; it touches
            // the old and new focus tiles plus a small budget-recovery margin.
            if (moves.size() > 24) bounded = false;

            // Re-applying the identical working set must not move any tile.
            const GovernorResult repeat = session.update(grid);
            if (!DiffPlans(cur, repeat).empty()) bounded = false;
            prev = repeat;
        }
        check(allInBudget, "every focus step stays within both high-water budgets");
        check(focusProtected, "the focused tile is Main at every step of the sweep");
        check(noPause, "all 64 tiles keep decoding across the sweep (none paused, no overflow)");
        check(bounded, "focus changes cause bounded churn and steady load does not flap");
    }

    // Scenario 12: honest per-tile state — the four states a UI must show so a
    // non-live tile is never presented as live.
    {
        // (a) Comfortably within budget: every tile is Live at its desired tier.
        std::vector<TileRequest> easy;
        for (int i = 0; i < 3; ++i) { TileRequest r; r.id = i; r.desired = Tier::Sub; easy.push_back(r); }
        const auto easyRes = Governor(DevBoxProfile()).assign(easy);
        std::cout << "\n== honest per-tile state ==\n";
        bool allLive = true;
        for (const auto& d : easyRes.tiles) if (d.state != TileState::Live) allLive = false;
        check(allLive, "within budget: every tile reports Live at its desired tier");

        // (b) Tight budget + off-screen tiles surface the other three states.
        CapacityProfile p;
        p.label = "tiny (state coverage)";
        p.decodeBudget = 0.35;          // high-water 0.315: only a fraction of one main-eq
        p.memoryBudgetMb = 1000000.0;   // make decode the sole constraint
        std::vector<TileRequest> req;
        for (int i = 0; i < 4; ++i) {   // visible, want Main
            TileRequest r; r.id = i; r.desired = Tier::Main; r.visible = true;
            r.priority = (i == 0) ? Priority::High : Priority::Medium;
            r.focused = (i == 0);
            req.push_back(r);
        }
        for (int i = 4; i < 6; ++i) {   // off-screen
            TileRequest r; r.id = i; r.desired = Tier::Main; r.visible = false;
            req.push_back(r);
        }
        const auto res = Governor(p).assign(req);
        for (const auto& d : res.tiles)
            std::cout << "  tile " << d.id << ": " << TierName(d.tier) << " / "
                      << TileStateName(d.state) << "\n";

        check(stateOf(res, 4) == TileState::PausedOffscreen &&
              stateOf(res, 5) == TileState::PausedOffscreen,
              "off-screen tiles report paused(offscreen)");
        int capacityPaused = 0;
        for (int i = 1; i <= 3; ++i)
            if (stateOf(res, i) == TileState::PausedCapacity) ++capacityPaused;
        check(capacityPaused >= 1 && res.overflow,
              "a visible tile that wanted video but got no budget reports paused(capacity)");
        check(tierOf(res, 0) != Tier::Paused && stateOf(res, 0) == TileState::Degraded,
              "the protected focused tile still decodes but below its desired tier -> degraded");
        bool consistent = true;
        for (const auto& d : res.tiles) {
            const bool isDegradedState = d.state == TileState::Degraded;
            const bool isDegradedFlag = d.tier != Tier::Paused && d.degraded;
            if (isDegradedState != isDegradedFlag) consistent = false;
        }
        check(consistent, "the Degraded state matches the degraded flag on every tile");
    }

    std::cout << "\n================================\n";
    if (g_failures == 0) {
        std::cout << "ALL CHECKS PASSED\n";
        return 0;
    }
    std::cout << g_failures << " CHECK(S) FAILED\n";
    return 1;
}
