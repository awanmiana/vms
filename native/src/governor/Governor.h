#pragma once

// P3-03 decode governor - pure decision core (native increment 4b).
//
// Given what the operator wants on screen and what THIS machine can sustain,
// decide a per-tile quality tier that stays within two independent budgets:
// decode throughput and memory. This is portable C++ with no GStreamer / OS
// dependency so it is unit-testable in isolation; the media layer consumes its
// decisions. Scope + rationale: ../governor-P3-03-proposal.md. Cost defaults are
// calibrated from ../MEASUREMENTS.md.

#include <string>
#include <vector>

namespace vms {

// Per-view media tier (quality of a visible tile). Ordered so a numeric compare
// means "more expensive": Paused < Thumb < Sub < Main.
enum class Tier { Paused = 0, Thumb = 1, Sub = 2, Main = 3 };

// Device activity priority (is a device worth the good tier under pressure?).
// This is the P0-01L axis; here it is used ONLY as a degrade tie-breaker.
enum class Priority { Idle = 0, Low = 1, Medium = 2, High = 3 };

const char* TierName(Tier t);
const char* PriorityName(Priority p);

// The honest signal a UI renders for a tile, so a non-live tile is never shown
// as if it were live. Derived from the tier the governor assigned vs what the
// tile requested. (The visual rendering of this is the P3-14 Qt/QML workspace;
// this enum is the state model that pass will consume.)
enum class TileState {
    Live,             // decoding at the requested tier
    Degraded,         // decoding, but below the requested tier (capacity pressure)
    PausedOffscreen,  // not visible: intentionally not decoding
    PausedCapacity,   // visible and wanted video, but capacity forced it off
};

const char* TileStateName(TileState s);

// Cost of decoding one stream at a tier: decode in "main-stream equivalents",
// memory in resident MB. Calibrated from the corrected I420 d3d12h265dec ramp
// (MEASUREMENTS.md, 2026-07-25): ~64 sustainable 1080p main vs ~249 sub streams
// => sub ~= 0.28 of a main. That path is decode-throughput-bound, not memory-
// bound: process RSS is low (~38MB/main stream; 64 main ran at 2.75GB, no crash),
// so per-tier memory is modeled resolution-monotonic as a safety net rather than
// as the binding wall (the old ~200MB/main figure was a pre-fix NVDEC artifact).
struct TierCost {
    double decode = 0.0;
    double memoryMb = 0.0;
    double bandwidthKbps = 0.0;   // network cost of the stream (0 = unmodeled)
};

struct CostModel {
    // Bandwidth defaults model a typical H.265 camera: ~4 Mbps main, ~1 Mbps sub,
    // ~0.25 Mbps thumb. Only enforced when the profile sets a bandwidth budget.
    TierCost main{1.0, 50.0, 4000.0};
    TierCost sub{0.28, 35.0, 1000.0};   // ~64/249 sustainable
    TierCost thumb{0.10, 20.0, 256.0};
    TierCost paused{0.0, 0.0, 0.0};
    const TierCost& forTier(Tier t) const;
};

// What this machine can sustain. decodeBudget is in main-equivalents (e.g. ~64
// on the dev box for hardware H.265 via d3d12h265dec). Watermarks give
// hysteresis: the governor degrades to keep usage under high*budget and leaves
// head-room before the real wall; a caller should only raise a desired tier
// while usage stays under low*budget, to avoid flapping.
struct CapacityProfile {
    double decodeBudget = 64.0;
    double memoryBudgetMb = 6000.0;
    // Network link budget in kbps. 0 = unmodeled/unlimited (the default, so the
    // two-budget behavior is unchanged); the optimizer (increment 8) sets this
    // from the estimated link so admission also respects bandwidth.
    double bandwidthBudgetKbps = 0.0;
    double highWatermark = 0.90;
    double lowWatermark = 0.75;
    bool hardwareDecode = true; // false => software/CPU-bound machine
    std::string label;
};

// A tile the layout would like on screen.
struct TileRequest {
    int id = 0;
    Tier desired = Tier::Main;          // focused=Main, visible=Sub, etc.
    Priority priority = Priority::Medium;
    bool focused = false;               // protected: downgraded only as last resort
    bool visible = true;                // off-screen => forced Paused (no decode)
};

struct TileDecision {
    int id = 0;
    Tier tier = Tier::Paused;
    bool degraded = false;      // assigned below its desired tier
    TileState state = TileState::Live;  // honest per-tile state for the UI
};

struct GovernorResult {
    std::vector<TileDecision> tiles;
    double decodeUsed = 0.0;    // main-equivalents
    double memoryUsedMb = 0.0;
    double bandwidthUsedKbps = 0.0;
    int decoding = 0;           // tiles not Paused
    bool overflow = false;      // a visible tile that wanted video had to be paused
};

// One tile's change between two successive plans, so a re-plan can be applied to
// a live media pipeline tile-by-tile. `from`/`to` are the tier before and after.
struct TileTransition {
    int id = 0;
    Tier from = Tier::Paused;
    Tier to = Tier::Paused;
    bool release() const { return to < from; }   // frees decode/memory budget
    bool acquire() const { return to > from; }   // consumes decode/memory budget
    bool stopsDecode() const { return from != Tier::Paused && to == Tier::Paused; }
    bool startsDecode() const { return from == Tier::Paused && to != Tier::Paused; }
};

// Diff two plans by tile id, returning one entry per tile whose tier changed. A
// tile present in only one plan is treated as arriving from / departing to
// Paused. The result is ordered so that applying it in sequence to a live
// pipeline never transiently exceeds capacity: every release (downgrade/pause,
// which frees budget) comes before every acquire (upgrade/resume, which spends
// it); ties break by id for determinism. An empty result means nothing changed.
std::vector<TileTransition> DiffPlans(const GovernorResult& before,
                                      const GovernorResult& after);

class Governor {
public:
    explicit Governor(CapacityProfile profile, CostModel costs = CostModel{});

    // Pure, deterministic: assign every requested tile a tier that fits within
    // budget, protecting focused/high-priority tiles and degrading the least
    // important tiles first. Never returns an assignment above the hard budget.
    GovernorResult assign(const std::vector<TileRequest>& requests) const;

    const CapacityProfile& profile() const { return profile_; }

private:
    CapacityProfile profile_;
    CostModel costs_;
};

// Stateful wrapper for a changing working set. Load increases and focus/priority
// changes are re-admitted against the high watermark immediately. Load decreases
// retain the current tiers and only recover quality while the resulting plan
// remains below the low watermark. Repeating the same inputs returns the same
// tiers, so steady load cannot flap.
class GovernorSession {
public:
    explicit GovernorSession(CapacityProfile profile, CostModel costs = CostModel{});

    GovernorResult update(const std::vector<TileRequest>& requests);
    void reset();

    const CapacityProfile& profile() const { return profile_; }
    bool initialized() const { return initialized_; }

private:
    CapacityProfile profile_;
    CostModel costs_;
    std::vector<TileRequest> previousRequests_;
    GovernorResult previousResult_;
    bool initialized_ = false;
};

// Named capacity profiles from MEASUREMENTS.md, for tests and explicit overrides.
CapacityProfile DevBoxProfile();   // RTX 3050, hardware H.265: measured ceilings
CapacityProfile LowEndProfile();   // i5 4th-gen no-GPU: PLACEHOLDER until real ramp

enum class Codec { H265, H264 };

// Static hardware facts (from vms_hwprobe or synthetic), kept as plain numbers so
// the governor stays free of any platform/probe dependency and unit-testable.
struct HardwareInputs {
    bool hasHardwareDecode = false; // HW decode available for the working codec
    double totalRamMb = 0.0;
    double videoMemoryMb = 0.0;     // dedicated VRAM; 0 if integrated/unknown
    unsigned int logicalCores = 0;
};

// Build a CONSERVATIVE capacity profile from static hardware facts. A decode-
// throughput / memory ceiling cannot be derived exactly from specs, so this
// seeds LOW on purpose (degrade early, never crash); a ramp-decode calibration
// refines it. See MEASUREMENTS.md.
CapacityProfile MakeCapacityProfile(const HardwareInputs& hw, const std::string& label);

} // namespace vms
