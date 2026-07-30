#pragma once

// ONVIF discovery-as-a-source — native increment 16 (P2-01 / P2-03 / P2-14).
// Scope: ../Development_plan.md (Approval Log "ONVIF discovery → onboard UI").
//
// The seam that lets the onboarding UI (DeviceController) drive the two already-
// built, fixture-verified ONVIF cores — WS-Discovery (increment 11) and the
// device/media SOAP client (increment 12) — WITHOUT the controller (or its
// offscreen tests) touching a socket. A DiscoverySource has two operations:
//
//   discover(timeout) -> the ONVIF devices answering on the LAN (candidates), and
//   fetch(serviceUrl, user, pass) -> one device's info + media profiles + RTSP
//                                    stream URIs (OnvifDevice, from OnvifTransport).
//
// The LIVE implementation (MakeLiveDiscoverySource) calls the real UDP + HTTP
// transports and is verified on hardware; a fixture-backed fake (in the workspace
// tests/demo) serves canned candidates + profiles so the whole discover -> map ->
// onboard flow verifies offscreen with no camera. Only these two calls perform
// network I/O, which is why they live behind this interface — the mapping from a
// fetched OnvifDevice to persisted channels, and duplicate detection against the
// existing inventory, are pure and fully testable.
//
// Pure C++ + Qt6::Core (QUrl host parse); no persist/health dependency.

#include <memory>
#include <string>
#include <vector>

#include "onvif/OnvifTransport.h"  // OnvifDevice (info + profiles + streamUris)

namespace vms::onvif {

// One ONVIF device answering discovery, before onboarding. `endpointRef` is the
// stable device EPR (the dedup key); `xaddr` is its ONVIF device-service URL
// (passed back into fetch()); `host` is the address derived from the xaddr,
// which onboarding uses to detect an already-onboarded device.
struct DiscoveryCandidate {
    std::string endpointRef;
    std::string name;        // decoded ONVIF "name" scope (may be empty)
    std::string hardware;    // decoded ONVIF "hardware" scope (may be empty)
    std::string xaddr;       // ONVIF device-service URL
    std::string host;        // host/IP parsed from the xaddr
};

// Injectable discovery + fetch. The live impl performs real network I/O; a fake
// serves fixtures so DeviceController and its offscreen tests never hit a socket.
class DiscoverySource {
public:
    virtual ~DiscoverySource() = default;

    // Discover ONVIF devices on the LAN, blocking up to `timeoutMs`, deduped by
    // endpoint. Sets `error` and returns empty on a transport failure.
    virtual std::vector<DiscoveryCandidate> discover(int timeoutMs,
                                                     std::string& error) = 0;

    // Fetch one device's info + media profiles + per-profile RTSP stream URIs,
    // authenticating with WS-Security UsernameToken. Returns false + sets
    // `error` on failure.
    virtual bool fetch(const std::string& serviceUrl, const std::string& user,
                       const std::string& password, OnvifDevice& out,
                       std::string& error) = 0;
};

// The live source over the real ONVIF transports (WsDiscoveryTransport +
// OnvifTransport). Returns nullptr when the transports were not compiled in
// (no Qt6::Network), so callers degrade honestly to "discovery unavailable".
std::unique_ptr<DiscoverySource> MakeLiveDiscoverySource();

// Parse the host/IP out of an ONVIF XAddr device-service URL
// ("http://192.168.0.254/onvif/device_service" -> "192.168.0.254"). Empty on a
// malformed URL. Exposed for reuse + testing.
std::string HostFromXAddr(const std::string& xaddr);

}  // namespace vms::onvif
