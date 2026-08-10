#pragma once

#include <string>
#include <vector>

#include "broker/ConnectionBroker.h"
#include "media/GridLiveSource.h"

namespace vms::media {

// Standalone-runtime adapter: exact tile order is supplied by the operator/UI,
// while ConnectionBroker remains the sole credential materialization identity.
class BrokerGridLiveSource final : public GridLiveSource {
public:
    BrokerGridLiveSource(vms::broker::ConnectionBroker& broker,
                         std::vector<std::string> cameraIds);

    vms::persist::Error acquire(int tileId, vms::Tier tier,
                                GridStreamLease& out) override;
    void release(GridStreamLease& lease, StreamOutcome outcome) override;
    int cameraCount() const override;

private:
    vms::broker::ConnectionBroker& broker_;
    std::vector<std::string> cameraIds_;
};

}  // namespace vms::media
