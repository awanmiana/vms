#pragma once

// P0-04 persistence — workspace layout repository (increment 5c).
//
// Saves and restores one stateful workspace instance's operator setup — its
// tile count and per-tile desired-tier + priority overrides — behind the C0-03
// contract (atomic replace via a transaction; a partial write never persists).
// The `Live` and `Playback` instances are stored under distinct names, matching
// the P3-14 "two independently stateful instances" model. Qt-free.

#include <optional>
#include <string>
#include <vector>

#include "persist/Store.h"

namespace vms::persist {

// One tile's persisted operator overrides. `desiredTier`/`priority` hold the
// vms::Tier / vms::Priority enum values as plain ints (the persist layer stays
// free of the governor's types).
struct TilePref {
    int id = 0;
    int desiredTier = 3;   // Main
    int priority = 2;      // Medium
    // Spatial-canvas world position (inc 24, P3-15). -1 = unset: the tile sits
    // at the default grid placement until the operator drags it.
    double posX = -1.0;
    double posY = -1.0;
    double facingDeg = 0.0;
    double fovDeg = 70.0;
};

struct InstanceState {
    int tileCount = 0;
    std::vector<TilePref> tiles;
};

class WorkspaceRepo {
public:
    explicit WorkspaceRepo(Store& store) : store_(store) {}

    // Atomically replace the stored state for `instance` ('live' | 'playback').
    Error save(const std::string& instance, const InstanceState& state);

    // Load `instance`. Sets `found=false` (and leaves `out` empty) when nothing
    // was ever saved for it — a first run, not an error.
    Error load(const std::string& instance, InstanceState& out, bool& found);

private:
    Store& store_;
};

} // namespace vms::persist
