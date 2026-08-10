#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace vms::media {

// RTSP is the session-control protocol. This policy selects the RTP/RTCP
// lower transport negotiated in SETUP; it does not confuse RTSP's TCP control
// connection with the media transport.
enum class RtspTransportMode {
    Auto,          // UDP unicast -> UDP multicast -> interleaved TCP
    Tcp,           // RTP/RTCP interleaved on the RTSP TCP connection
    UdpUnicast,    // separate negotiated unicast RTP/RTCP UDP ports
    UdpMulticast   // server-selected multicast RTP/RTCP groups
};

struct RtspTransportPolicy {
    RtspTransportMode mode = RtspTransportMode::Auto;
    std::uint32_t latencyMs = 150;
    std::uint64_t udpFallbackTimeoutUs = 5'000'000;
    std::uint64_t tcpTimeoutUs = 20'000'000;
    bool sendRtcp = true;
};

// GStreamer's GstRTSPLowerTrans values mirror the RTSP lower-transport choices:
// UDP=1, UDP multicast=2, TCP interleaved=4. Keeping the mapping in the pure
// policy core makes it independently testable and avoids magic values at call sites.
std::uint32_t RtspLowerTransportMask(RtspTransportMode mode);
const char* RtspTransportModeName(RtspTransportMode mode);
bool ParseRtspTransportMode(std::string_view text, RtspTransportMode& mode);

// Reject pathological command-line/configuration values before they reach a
// live pipeline. Zero timeouts deliberately mean "disabled", matching rtspsrc.
bool ValidateRtspTransportPolicy(const RtspTransportPolicy& policy,
                                 std::string& error);

}  // namespace vms::media
