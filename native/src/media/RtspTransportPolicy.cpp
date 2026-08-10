#include "media/RtspTransportPolicy.h"

#include <algorithm>
#include <cctype>

namespace vms::media {

std::uint32_t RtspLowerTransportMask(RtspTransportMode mode) {
    switch (mode) {
        case RtspTransportMode::Auto: return 0x1U | 0x2U | 0x4U;
        case RtspTransportMode::Tcp: return 0x4U;
        case RtspTransportMode::UdpUnicast: return 0x1U;
        case RtspTransportMode::UdpMulticast: return 0x2U;
    }
    return 0U;
}

const char* RtspTransportModeName(RtspTransportMode mode) {
    switch (mode) {
        case RtspTransportMode::Auto: return "auto";
        case RtspTransportMode::Tcp: return "tcp";
        case RtspTransportMode::UdpUnicast: return "udp";
        case RtspTransportMode::UdpMulticast: return "multicast";
    }
    return "unknown";
}

bool ParseRtspTransportMode(std::string_view text, RtspTransportMode& mode) {
    std::string value(text);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (value == "auto") mode = RtspTransportMode::Auto;
    else if (value == "tcp" || value == "tcp-interleaved") mode = RtspTransportMode::Tcp;
    else if (value == "udp" || value == "udp-unicast") mode = RtspTransportMode::UdpUnicast;
    else if (value == "multicast" || value == "udp-multicast")
        mode = RtspTransportMode::UdpMulticast;
    else return false;
    return true;
}

bool ValidateRtspTransportPolicy(const RtspTransportPolicy& policy,
                                 std::string& error) {
    if (RtspLowerTransportMask(policy.mode) == 0U) {
        error = "unknown RTSP lower transport";
        return false;
    }
    if (policy.latencyMs > 60'000U) {
        error = "RTSP latency must be between 0 and 60000 ms";
        return false;
    }
    constexpr std::uint64_t kMaxTimeoutUs = 300'000'000ULL;
    if (policy.udpFallbackTimeoutUs > kMaxTimeoutUs ||
        policy.tcpTimeoutUs > kMaxTimeoutUs) {
        error = "RTSP transport timeouts must be between 0 and 300000 ms";
        return false;
    }
    error.clear();
    return true;
}

}  // namespace vms::media
