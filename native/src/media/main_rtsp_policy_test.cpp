#include "media/RtspTransportPolicy.h"

#include <iostream>
#include <string>

int main() {
    using namespace vms::media;
    int checks = 0;
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        ++checks;
        if (!ok) { ++failures; std::cerr << "FAIL: " << what << "\n"; }
    };

    check(RtspLowerTransportMask(RtspTransportMode::Auto) == 7U,
          "auto permits every standardized lower transport");
    check(RtspLowerTransportMask(RtspTransportMode::Tcp) == 4U,
          "TCP maps to interleaved RTP/RTCP");
    check(RtspLowerTransportMask(RtspTransportMode::UdpUnicast) == 1U,
          "UDP maps to unicast RTP/RTCP");
    check(RtspLowerTransportMask(RtspTransportMode::UdpMulticast) == 2U,
          "multicast is distinct from unicast UDP");

    RtspTransportMode mode = RtspTransportMode::Auto;
    check(ParseRtspTransportMode("tcp", mode) && mode == RtspTransportMode::Tcp,
          "parse TCP");
    check(ParseRtspTransportMode("TCP-INTERLEAVED", mode) &&
              mode == RtspTransportMode::Tcp,
          "parse case-insensitive TCP alias");
    check(ParseRtspTransportMode("udp-unicast", mode) &&
              mode == RtspTransportMode::UdpUnicast,
          "parse UDP unicast");
    check(ParseRtspTransportMode("udp-multicast", mode) &&
              mode == RtspTransportMode::UdpMulticast,
          "parse UDP multicast");
    check(!ParseRtspTransportMode("http", mode), "reject non-RTSP transport");

    RtspTransportPolicy policy;
    std::string error;
    check(ValidateRtspTransportPolicy(policy, error) && error.empty(),
          "default policy is valid");
    policy.latencyMs = 60'001U;
    check(!ValidateRtspTransportPolicy(policy, error), "reject excessive jitter buffer");
    policy.latencyMs = 0U;
    policy.tcpTimeoutUs = 0U;
    check(ValidateRtspTransportPolicy(policy, error), "zero latency/timeout are explicit");
    policy.udpFallbackTimeoutUs = 300'000'001ULL;
    check(!ValidateRtspTransportPolicy(policy, error), "reject excessive UDP fallback");

    std::cout << (failures == 0 ? "PASS: " : "FAILED: ") << checks
              << " RTSP transport-policy checks";
    if (failures) std::cout << " (" << failures << " failed)";
    std::cout << "\n";
    return failures == 0 ? 0 : 1;
}
