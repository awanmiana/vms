// Increment 3 — RTSP decode + render spike.
//
// Opens one RTSP stream, decodes it (preferring a hardware d3d11 decoder when
// available), renders via Direct3D 11, and reports which decoder was selected,
// frames rendered/dropped, and time-to-first-frame. This is the measurement
// that validates the stack against the zero-latency target on real hardware.
//
// Usage:
//   vms_spike "rtsp://user:pass%40host:port/path" [--seconds N]
//   (or set VMS_RTSP_URL; --seconds 0 runs until the window/console is closed)
//
// Credentials are supplied at run time only and are never stored or committed.

#include <gst/gst.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {

struct SpikeState {
    GstElement* pipeline = nullptr;
    GstElement* fpssink = nullptr;
    GMainLoop* loop = nullptr;
    gint64 playingUs = 0;
    bool firstFrameSeen = false;
    bool decodersPrinted = false;
};

// Lower the RTSP jitter buffer and prefer TCP (more reliable through NAT/GRE).
void OnSourceSetup(GstElement* /*playbin*/, GstElement* source, gpointer /*data*/) {
    GObjectClass* klass = G_OBJECT_GET_CLASS(source);
    if (g_object_class_find_property(klass, "latency"))
        g_object_set(source, "latency", static_cast<guint>(150), nullptr);
    if (g_object_class_find_property(klass, "protocols"))
        g_object_set(source, "protocols", 0x4 /* GST_RTSP_LOWER_TRANS_TCP */, nullptr);
}

// Print decoder/relevant element factory names so we can confirm hardware
// (d3d11h264dec / d3d11h265dec) vs software (avdec_*) decode was chosen.
void PrintDecoders(GstBin* bin) {
    GstIterator* it = gst_bin_iterate_recurse(bin);
    GValue item = G_VALUE_INIT;
    gboolean done = FALSE;
    while (!done) {
        switch (gst_iterator_next(it, &item)) {
            case GST_ITERATOR_OK: {
                GstElement* e = GST_ELEMENT(g_value_get_object(&item));
                GstElementFactory* f = gst_element_get_factory(e);
                if (f) {
                    const gchar* name =
                        gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(f));
                    if (name && (g_strrstr(name, "dec") || g_strrstr(name, "d3d11")))
                        std::cout << "  active element: " << name << "\n";
                }
                g_value_reset(&item);
                break;
            }
            case GST_ITERATOR_RESYNC: gst_iterator_resync(it); break;
            case GST_ITERATOR_ERROR:
            case GST_ITERATOR_DONE:  done = TRUE; break;
        }
    }
    g_value_unset(&item);
    gst_iterator_free(it);
}

gboolean StatsTick(gpointer user_data) {
    auto* s = static_cast<SpikeState*>(user_data);
    guint64 rendered = 0, dropped = 0;
    g_object_get(s->fpssink, "frames-rendered", &rendered,
                 "frames-dropped", &dropped, nullptr);

    if (!s->firstFrameSeen && rendered > 0) {
        s->firstFrameSeen = true;
        const double ms = (g_get_monotonic_time() - s->playingUs) / 1000.0;
        std::cout << "first frame after " << ms << " ms\n";
    }
    if (!s->decodersPrinted && rendered > 0) {
        s->decodersPrinted = true;
        std::cout << "selected decode path:\n";
        PrintDecoders(GST_BIN(s->pipeline));
    }
    std::cout << "frames rendered=" << rendered << " dropped=" << dropped << std::endl;
    return TRUE; // keep firing
}

gboolean StopAfterTimeout(gpointer user_data) {
    auto* s = static_cast<SpikeState*>(user_data);
    std::cout << "-- time limit reached --\n";
    g_main_loop_quit(s->loop);
    return FALSE;
}

gboolean BusCb(GstBus* /*bus*/, GstMessage* msg, gpointer user_data) {
    auto* s = static_cast<SpikeState*>(user_data);
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr; gchar* dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            std::cerr << "ERROR: " << (err ? err->message : "?") << "\n";
            if (dbg) std::cerr << "  debug: " << dbg << "\n";
            g_clear_error(&err); g_free(dbg);
            g_main_loop_quit(s->loop);
            break;
        }
        case GST_MESSAGE_EOS:
            std::cout << "EOS\n";
            g_main_loop_quit(s->loop);
            break;
        case GST_MESSAGE_STATE_CHANGED:
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(s->pipeline)) {
                GstState oldS, newS, pend;
                gst_message_parse_state_changed(msg, &oldS, &newS, &pend);
                std::cout << "pipeline: " << gst_element_state_get_name(oldS)
                          << " -> " << gst_element_state_get_name(newS) << "\n";
                if (newS == GST_STATE_PLAYING && s->playingUs == 0)
                    s->playingUs = g_get_monotonic_time();
            }
            break;
        default: break;
    }
    return TRUE;
}

} // namespace

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    std::string url;
    int seconds = 20;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--seconds" && i + 1 < argc) { seconds = std::atoi(argv[++i]); }
        else if (a.rfind("--", 0) != 0)       { url = a; }
    }
    if (url.empty()) {
        if (const char* e = std::getenv("VMS_RTSP_URL")) url = e;
    }
    if (url.empty()) {
        std::cerr << "Usage: vms_spike \"rtsp://user:pass%40host:port/path\" [--seconds N]\n"
                     "       (or set VMS_RTSP_URL). Encode any '@' in the password as %40.\n";
        return 2;
    }

    SpikeState s;
    s.loop = g_main_loop_new(nullptr, FALSE);

    // playbin auto-negotiates depay/parse/decode and selects a hardware decoder
    // by rank when available. We wrap the sink in fpsdisplaysink for stats.
    s.pipeline = gst_element_factory_make("playbin", "playbin");
    GstElement* d3dsink = gst_element_factory_make("d3d11videosink", "d3dsink");
    s.fpssink = gst_element_factory_make("fpsdisplaysink", "fps");
    if (!s.pipeline || !d3dsink || !s.fpssink) {
        std::cerr << "Failed to create elements (is GStreamer on PATH with the "
                     "d3d11 + fpsdisplaysink plugins?)\n";
        return 3;
    }

    g_object_set(s.fpssink, "video-sink", d3dsink, "text-overlay", FALSE, nullptr);
    g_object_set(s.pipeline, "uri", url.c_str(), "video-sink", s.fpssink, nullptr);
    g_signal_connect(s.pipeline, "source-setup", G_CALLBACK(OnSourceSetup), nullptr);

    GstBus* bus = gst_element_get_bus(s.pipeline);
    gst_bus_add_watch(bus, BusCb, &s);
    gst_object_unref(bus);

    std::cout << "connecting to RTSP source (credentials not shown)...\n";
    if (gst_element_set_state(s.pipeline, GST_STATE_PLAYING) ==
        GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Failed to set pipeline to PLAYING\n";
        return 4;
    }

    g_timeout_add_seconds(1, StatsTick, &s);
    if (seconds > 0) g_timeout_add_seconds(seconds, StopAfterTimeout, &s);

    g_main_loop_run(s.loop);

    // Final tally.
    guint64 rendered = 0, dropped = 0;
    g_object_get(s.fpssink, "frames-rendered", &rendered,
                 "frames-dropped", &dropped, nullptr);
    std::cout << "SUMMARY: rendered=" << rendered << " dropped=" << dropped << std::endl;

    gst_element_set_state(s.pipeline, GST_STATE_NULL);
    gst_object_unref(s.pipeline);
    g_main_loop_unref(s.loop);
    return 0;
}
