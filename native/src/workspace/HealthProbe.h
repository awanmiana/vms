#pragma once

// Device-health live feed — native increment 19 (P2-13). Scope:
// ../Development_plan.md (Approval Log "Device health & lifecycle" — the live
// feed named as the increment-13/14 remainder: "feed health from real broker
// connect outcomes + ONVIF/live polling").
//
// The seam that lets DeviceController drive the HealthMonitor (increment 13)
// automatically instead of only when an operator manually reports a dimension.
// A DeviceHealthProbe answers one honest question per device — "is it reachable
// right now?" — and the controller maps the answer onto reportReachable, so a
// device's state goes Unknown -> Online / Offline on its own.
//
// This is deliberately REACHABILITY ONLY: a TCP connect to the device's media
// port proves the host answers, not that a stream decodes, so stream health
// stays Unknown (reach Online + stream Unknown derives Online honestly; proving
// stream Ok needs a real media open, a later media gate). The probe is injected
// so the controller + its offscreen tests never touch a socket: MakeTcpHealthProbe
// is the live implementation (Qt Network); a fake serves scripted outcomes.
//
// Pure interface (no Qt) so DeviceController.h can include it freely; the live
// implementation lives in HealthProbe.cpp (Qt6::Network).

#include <memory>
#include <string>

namespace vms::health {

// One device to probe: its stable id + the host/port to reach (the media port,
// default RTSP 554, parsed from the device address).
struct ProbeTarget {
    std::string deviceId;
    std::string host;
    int port = 554;
};

// The honest outcome of one probe. `reachable` is false on any failure/timeout.
struct ProbeResult {
    std::string deviceId;
    bool reachable = false;
};

// Injectable reachability probe. The live impl performs a blocking TCP connect;
// a fake serves fixtures so DeviceController + tests stay socket-free.
class DeviceHealthProbe {
public:
    virtual ~DeviceHealthProbe() = default;
    virtual ProbeResult probe(const ProbeTarget& target) = 0;
};

// The live TCP-reachability probe (Qt Network). `timeoutMs` bounds each connect.
std::unique_ptr<DeviceHealthProbe> MakeTcpHealthProbe(int timeoutMs = 800);

}  // namespace vms::health
