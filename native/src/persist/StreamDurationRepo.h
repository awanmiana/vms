#pragma once

// P3-18 / native increment 39: persistent, premises-scoped decoded stream
// branch-time. Elapsed time is supplied by a monotonic in-process observer;
// this repository never derives duration from wall-clock timestamps or fills
// gaps across a restart.

#include <cstdint>
#include <string>

#include "persist/Store.h"

namespace vms::persist {

struct StreamDurationSummary {
    std::int64_t milliseconds = 0;  // summed branch-time
    std::int64_t checkpoints = 0;
    std::string updatedAtUtc;
};

class StreamDurationRepo {
public:
    explicit StreamDurationRepo(Store& store) : store_(store) {}

    // Atomically add elapsedMilliseconds * playingBranches. A zero elapsed
    // interval or zero playing branches is a successful no-op. Negative values,
    // an empty site id, and arithmetic overflow are refused without a write.
    Error addObservation(const std::string& siteId,
                         std::int64_t elapsedMilliseconds,
                         int playingBranches);

    // Read the persisted total. A valid site with no observations returns a
    // successful zero summary: the source was queried and found no evidence.
    Error summary(const std::string& siteId, StreamDurationSummary& out);

private:
    Store& store_;
};

}  // namespace vms::persist
