#pragma once

// ONVIF device/media SOAP client — native increment 12 (P2-14), completing the
// ONVIF onboarding path begun by WS-Discovery (increment 11). Given a device's
// ONVIF service URL + credentials, it fetches device info + media profiles +
// per-profile RTSP stream URIs, which the orchestrator turns into DeviceRepo rows.
//
// This header is the PURE half: build the SOAP requests (with a standards-exact
// WS-Security UsernameToken PasswordDigest header) and parse the responses. No
// HTTP — so it is unit-testable against captured device fixtures. The live HTTP
// transport + onboarding orchestration are in OnvifTransport. Parsing is
// namespace-prefix-agnostic (by element local name) like WsDiscovery.
//
// Standards: ONVIF Core/Media (Device & Media wsdl), OASIS WSS UsernameToken
// Profile (PasswordDigest = Base64(SHA1( base64decode(Nonce) + Created + Password ))).

#include <string>
#include <vector>

namespace vms::onvif {

struct MediaProfile {
    std::string token;
    std::string name;
    std::string encoding;   // H264 / H265 / JPEG
    int width = 0;
    int height = 0;
};

struct DeviceInformation {
    std::string manufacturer;
    std::string model;
    std::string firmware;
    std::string serial;
    std::string hardwareId;
};

// WSSE PasswordDigest. Deterministic for a given (nonce, created), so testable.
std::string PasswordDigest(const std::string& password,
                           const std::string& nonceBase64,
                           const std::string& createdUtc);

// The <wsse:Security> UsernameToken header block (embedded in each request).
std::string BuildSecurityHeader(const std::string& user, const std::string& password,
                                const std::string& nonceBase64,
                                const std::string& createdUtc);

// SOAP request envelopes. Each carries the WS-Security header above.
std::string BuildGetDeviceInformationRequest(const std::string& user,
                                             const std::string& password,
                                             const std::string& nonceBase64,
                                             const std::string& createdUtc);
std::string BuildGetProfilesRequest(const std::string& user, const std::string& password,
                                    const std::string& nonceBase64,
                                    const std::string& createdUtc);
std::string BuildGetStreamUriRequest(const std::string& profileToken,
                                     const std::string& user, const std::string& password,
                                     const std::string& nonceBase64,
                                     const std::string& createdUtc);

// Response parsers (prefix-agnostic; honest/empty on malformed input).
std::vector<MediaProfile> ParseProfiles(const std::string& xml);
std::string ParseStreamUri(const std::string& xml);
DeviceInformation ParseDeviceInformation(const std::string& xml);

} // namespace vms::onvif
