#pragma once

// ONVIF WS-Discovery — native increment 11 (P2-03). Scope: ../Development_plan.md
// (Approval Log "ONVIF discovery + interoperability").
//
// The standards-exact device-discovery source that feeds DeviceRepo (increment
// 10). ONVIF adopts OASIS WS-Discovery: the client multicasts a SOAP Probe for
// `dn:NetworkVideoTransmitter` to 239.255.255.250:3702, and each device answers
// with a ProbeMatch carrying its endpoint reference, Types, Scopes (name /
// hardware / location), and XAddrs (its ONVIF device-service URL). This header is
// the PURE half — build the Probe, parse ProbeMatches — with no sockets, so it is
// unit-testable against real device fixtures. The UDP transport is WsDiscoveryTransport.
//
// Parsing is namespace-prefix-agnostic (matches by element local name), because
// real devices vary the prefixes (d:, wsdd:, default ns) they emit.

#include <string>
#include <vector>

namespace vms::onvif {

// Build the ONVIF WS-Discovery Probe envelope. `messageId` must be a unique
// "urn:uuid:<uuid>" (the transport generates one; tests pass a fixed value so the
// output is deterministic). Standards: WS-Discovery 2005/04 + ONVIF Core.
std::string BuildProbe(const std::string& messageId);

// One device answering a Probe.
struct ProbeMatch {
    std::string endpointRef;            // wsa:Address (stable device EPR, urn:uuid:...)
    std::vector<std::string> types;     // e.g. "dn:NetworkVideoTransmitter"
    std::vector<std::string> scopes;    // "onvif://www.onvif.org/name/..." etc.
    std::vector<std::string> xaddrs;    // device-service URLs (http://ip/onvif/device_service)
};

// Parse one ProbeMatches SOAP datagram into zero or more matches. Returns empty
// on malformed/empty input (never throws) — honest, not a crash.
std::vector<ProbeMatch> ParseProbeMatches(const std::string& xml);

// Remove duplicate devices (same endpointRef), keeping the first seen. Devices
// answer more than once and on multiple interfaces, so a discovery pass dedups.
std::vector<ProbeMatch> DedupByEndpoint(const std::vector<ProbeMatch>& matches);

// The decoded value of the ONVIF scope `key` (e.g. "name", "hardware",
// "location") for a match, or empty if absent. ONVIF scopes are
// "onvif://www.onvif.org/<key>/<percent-encoded-value>".
std::string ScopeValue(const ProbeMatch& match, const std::string& key);

} // namespace vms::onvif
