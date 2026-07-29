#pragma once

// ONVIF device/media HTTP transport + onboarding fetch — native increment 12.
// The live half of the ONVIF client: POST the SOAP requests (OnvifClient) to a
// device's ONVIF service over HTTP and parse the replies into device info +
// media profiles + per-profile RTSP stream URIs. Kept separate from the pure
// OnvifClient so the SOAP build/parse logic stays HTTP-free and unit-testable.
// Performs real network I/O; verified on hardware against the test camera.

#include <string>
#include <vector>

#include "onvif/OnvifClient.h"

namespace vms::onvif {

// Everything onboarding needs from one ONVIF device.
struct OnvifDevice {
    DeviceInformation info;
    std::vector<MediaProfile> profiles;
    std::vector<std::string> streamUris;   // parallel to profiles (RTSP, credential-free)
};

// POST `soap` to `serviceUrl` (SOAP 1.2) and return the response body. Sets
// `error` and returns false on transport/HTTP failure.
bool OnvifCall(const std::string& serviceUrl, const std::string& soap,
               std::string& responseXml, std::string& error, int timeoutMs = 5000);

// Fetch device information, media profiles, and each profile's RTSP stream URI
// from the device's ONVIF service URL (as discovered via WS-Discovery XAddrs),
// authenticating with WS-Security UsernameToken. Nonce + created are generated
// per call. On success `out` is populated; on failure `error` is set.
bool FetchDevice(const std::string& serviceUrl, const std::string& user,
                 const std::string& password, OnvifDevice& out, std::string& error);

} // namespace vms::onvif
