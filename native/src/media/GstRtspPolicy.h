#pragma once

#include "media/RtspTransportPolicy.h"

#include <gst/gst.h>

namespace vms::media {

// uridecodebin/playbin can create many source types. Apply only properties the
// created source exposes, which makes the callback safe for non-RTSP URIs while
// keeping RTSP behavior explicit when the source is rtspsrc.
inline bool ApplyRtspTransportPolicy(GstElement* source,
                                     const RtspTransportPolicy& policy) {
    if (!source) return false;
    GObjectClass* klass = G_OBJECT_GET_CLASS(source);
    if (!g_object_class_find_property(klass, "protocols")) return false;

    g_object_set(source, "protocols",
                 static_cast<guint>(RtspLowerTransportMask(policy.mode)), nullptr);
    if (g_object_class_find_property(klass, "latency"))
        g_object_set(source, "latency", static_cast<guint>(policy.latencyMs), nullptr);
    if (g_object_class_find_property(klass, "timeout"))
        g_object_set(source, "timeout",
                     static_cast<guint64>(policy.udpFallbackTimeoutUs), nullptr);
    if (g_object_class_find_property(klass, "tcp-timeout"))
        g_object_set(source, "tcp-timeout",
                     static_cast<guint64>(policy.tcpTimeoutUs), nullptr);
    if (g_object_class_find_property(klass, "do-rtcp"))
        g_object_set(source, "do-rtcp", policy.sendRtcp ? TRUE : FALSE, nullptr);
    return true;
}

}  // namespace vms::media
