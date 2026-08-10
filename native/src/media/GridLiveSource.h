#pragma once

#include <cstdint>
#include <string>

#include "governor/Governor.h"
#include "persist/Error.h"

namespace vms::media {

// The media pipeline holds one lease only while a governed branch owns a real
// device stream. URI is credential-bearing and must never be logged or persisted.
struct GridStreamLease {
    std::uint64_t id = 0;
    std::string cameraId;
    std::string deviceId;
    std::string uri;
    bool valid = false;
};

enum class StreamOutcome {
    Unknown,  // released before the transport produced evidence
    Success,  // decoded media was observed
    Failure   // the source/transport reported a failure
};

// Portable provider seam: GridPipeline knows camera identity and tier but not
// SQLite, DPAPI, vendor credentials, or connection-pool implementation.
class GridLiveSource {
public:
    virtual ~GridLiveSource() = default;

    virtual vms::persist::Error acquire(int tileId, vms::Tier tier,
                                        GridStreamLease& out) = 0;
    virtual void release(GridStreamLease& lease, StreamOutcome outcome) = 0;
    virtual int cameraCount() const = 0;
};

// Best-effort in-memory zeroization for the credential-bearing URI. This does
// not invalidate the lease; release() still needs its non-secret identity fields.
inline void scrubLeaseUri(GridStreamLease& lease) {
    lease.uri.assign(lease.uri.size(), '\0');
    lease.uri.clear();
    lease.uri.shrink_to_fit();
}

}  // namespace vms::media
