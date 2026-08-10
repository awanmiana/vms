#include "media/BrokerGridLiveSource.h"

#include <utility>

namespace vms::media {

using vms::broker::Session;
using vms::broker::StreamKind;
using vms::persist::Error;
using vms::persist::Status;

BrokerGridLiveSource::BrokerGridLiveSource(
    vms::broker::ConnectionBroker& broker, std::vector<std::string> cameraIds)
    : broker_(broker), cameraIds_(std::move(cameraIds)) {}

Error BrokerGridLiveSource::acquire(int tileId, vms::Tier tier,
                                    GridStreamLease& out) {
    out = {};
    if (tileId < 0 || tileId >= static_cast<int>(cameraIds_.size()))
        return {Status::NotFound, "no camera assigned to tile " +
                                      std::to_string(tileId)};
    if (tier == vms::Tier::Paused)
        return {Status::Misuse, "paused tile cannot acquire a media stream"};

    // Thumb intentionally uses the real sub profile. Falling back to Main when
    // Sub is absent would violate the governor's capacity/bandwidth accounting.
    const StreamKind kind = tier == vms::Tier::Main ? StreamKind::Main
                                                     : StreamKind::Sub;
    Session session;
    if (Error e = broker_.connect(cameraIds_[tileId], kind, session); !e)
        return e;

    out.id = session.id;
    out.cameraId = std::move(session.cameraId);
    out.deviceId = std::move(session.deviceId);
    out.uri = std::move(session.url);
    out.valid = session.valid;
    return Error::success();
}

void BrokerGridLiveSource::release(GridStreamLease& lease,
                                   StreamOutcome outcome) {
    if (!lease.valid) {
        scrubLeaseUri(lease);
        lease = {};
        return;
    }

    if (outcome == StreamOutcome::Success)
        broker_.reportSuccess(lease.cameraId);
    else if (outcome == StreamOutcome::Failure)
        broker_.reportFailure(lease.cameraId);

    Session session;
    session.id = lease.id;
    session.cameraId = lease.cameraId;
    session.deviceId = lease.deviceId;
    session.valid = true;
    broker_.release(session);

    scrubLeaseUri(lease);
    lease = {};
}

int BrokerGridLiveSource::cameraCount() const {
    return static_cast<int>(cameraIds_.size());
}

}  // namespace vms::media
