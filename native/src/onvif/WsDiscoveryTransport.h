#pragma once

// ONVIF WS-Discovery UDP transport — native increment 11 (P2-03). The live half
// of discovery: multicast the Probe on the LAN and collect ProbeMatches. Kept
// separate from the pure WsDiscovery core so the parse/build logic stays
// socket-free and unit-testable. Live-verified on hardware (needs ONVIF devices
// on the network); build-verified here.

#include <string>
#include <vector>

namespace vms::onvif {

// A device found on the LAN, with the display fields decoded from its scopes.
struct DiscoveredDevice {
    std::string endpointRef;   // stable device EPR
    std::string name;
    std::string hardware;
    std::string xaddr;         // first ONVIF device-service URL
};

// Multicast an ONVIF Probe to 239.255.255.250:3702 and collect answers for
// `timeoutMs`, deduped by endpoint. Sets `error` and returns empty on a socket
// failure. This performs real network I/O.
std::vector<DiscoveredDevice> Discover(int timeoutMs, std::string& error);

} // namespace vms::onvif
