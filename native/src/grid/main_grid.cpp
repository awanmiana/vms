// Increment 4 (spike half) — multi-tile grid decode/render measurement.
//
// Decodes N RTSP streams (each preferring a hardware d3d11/d3d12 decoder when
// available), composites them into one grid on a single Direct3D 11 surface,
// and reports per-tile decode fps, the decoder each tile selected, aggregate
// frames rendered/dropped, and this process's CPU% and working set.
//
// This is a MEASUREMENT spike, not the governor: it applies no priority states,
// no per-view tiers, and no degrade/recover policy. Its job is to find the
// smooth ceiling per hardware tier — the empirical input the P3-03 / P0-05
// discussions need before any decode-governor policy is designed.
//
// Usage:
//   vms_grid "rtsp://u:p%40host:port/path" [more urls...] [--count N]
//            [--seconds N] [--width W] [--height H]
//   --count N   replicate the single provided URL N times (ramp to find ceiling)
//   --seconds N run time; 0 = until the window/console is closed (default 30)
//   (or set VMS_RTSP_URL for a single source)
//
// Camera-free fan-out (to measure the PURE decode+composite ceiling of a
// machine, without a camera's session cap or a single low-res sub-stream):
//   vms_grid --test-pattern --count N [--codec h265|h264]
//            [--srcw W] [--srch H] [--seconds N]
//   Encodes one synthetic clip up front, then fans out N decode-only branches
//   from it. The video sink runs with sync OFF, so each tile decodes as fast
//   as the hardware allows: per-tile fps is the max decode throughput, and
//   sum(fps)/25 estimates how many realtime 25 fps streams this box sustains.
//   Use --codec h264 on machines with no H.265 hardware decode (e.g. the
//   low-end i5 tier) to measure their QuickSync/DXVA H.264 ceiling instead.
//
// Credentials are supplied at run time only and are never stored or committed.

#include <gst/gst.h>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace {

struct Branch {
    int index = 0;
    GstElement* source = nullptr;     // multifilesrc in --test-pattern mode (unused for RTSP)
    GstElement* decodebin = nullptr;  // uridecodebin (RTSP) or decodebin (--test-pattern)
    GstElement* queue = nullptr;
    GstElement* upload = nullptr;
    GstPad* compPad = nullptr;
    std::atomic<guint64> buffers{0};  // decoded frames seen (written on stream thread)
    guint64 lastBuffers = 0;
    bool linked = false;
    bool failed = false;
    std::string decoder = "(pending)";
};

struct GridState {
    GstElement* pipeline = nullptr;
    GstElement* comp = nullptr;
    GstElement* fpssink = nullptr;
    GMainLoop* loop = nullptr;
    std::vector<Branch*> branches;
    gint64 playingUs = 0;
    bool firstFrameSeen = false;
#ifdef _WIN32
    ULARGE_INTEGER lastKernel{}, lastUser{};
    ULARGE_INTEGER lastWall{};
    DWORD numProcs = 1;
#endif
};

// Returns the first available element factory name from a null-terminated
// list, or nullptr if none are registered.
const char* PickFactory(const char* const* names) {
    for (const char* const* p = names; *p; ++p) {
        if (GstElementFactory* f = gst_element_factory_find(*p)) {
            gst_object_unref(f);
            return *p;
        }
    }
    return nullptr;
}

// Encodes one synthetic clip so --test-pattern can fan out N decode-only
// branches with no camera involved. A short H.264/H.265 elementary stream is
// written to outPath (looped by multifilesrc at run time). Returns true on
// success and reports which encoder was used. This runs ONCE, fully before
// the grid starts, so its cost never pollutes the decode measurement.
bool EncodePattern(const std::string& codec, int sw, int sh, int frames,
                   const std::string& outPath, std::string& encoderUsed) {
    const char* h265[] = {"x265enc", "qsvh265enc", "nvh265enc", nullptr};
    const char* h264[] = {"x264enc", "qsvh264enc", "nvh264enc", nullptr};
    const bool isH264 = (codec == "h264");
    const char* parseName = isH264 ? "h264parse" : "h265parse";
    const char* encName = PickFactory(isH264 ? h264 : h265);
    if (!encName) {
        std::cerr << "No " << codec << " encoder available (need x26"
                  << (isH264 ? "4" : "5") << "enc or a hardware encoder) "
                     "to build the test pattern.\n";
        return false;
    }
    encoderUsed = encName;

    GstElement* pipe  = gst_pipeline_new("encode");
    GstElement* src   = gst_element_factory_make("videotestsrc", nullptr);
    GstElement* conv  = gst_element_factory_make("videoconvert", nullptr);
    GstElement* caps  = gst_element_factory_make("capsfilter", nullptr);
    GstElement* enc   = gst_element_factory_make(encName, nullptr);
    GstElement* parse = gst_element_factory_make(parseName, nullptr);
    GstElement* sink  = gst_element_factory_make("filesink", nullptr);
    if (!pipe || !src || !conv || !caps || !enc || !parse || !sink) {
        std::cerr << "Failed to create encode elements.\n";
        if (pipe) gst_object_unref(pipe);
        return false;
    }

    g_object_set(src, "num-buffers", frames, "pattern", 0 /*smpte*/,
                 "is-live", FALSE, nullptr);
    GstCaps* c = gst_caps_new_simple(
        "video/x-raw", "width", G_TYPE_INT, sw, "height", G_TYPE_INT, sh,
        "framerate", GST_TYPE_FRACTION, 25, 1, nullptr);
    g_object_set(caps, "caps", c, nullptr);
    gst_caps_unref(c);
    // Frequent IDRs so multifilesrc's loop wrap re-syncs the decoder quickly.
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(enc), "key-int-max"))
        g_object_set(enc, "key-int-max", 25, nullptr);
    // Repeat SPS/PPS with every IDR so a mid-stream (looped) start decodes.
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(parse), "config-interval"))
        g_object_set(parse, "config-interval", -1, nullptr);
    g_object_set(sink, "location", outPath.c_str(), nullptr);

    gst_bin_add_many(GST_BIN(pipe), src, conv, caps, enc, parse, sink, nullptr);
    // Force the parser to emit an Annex-B byte-stream (start codes) so the raw
    // file re-typefinds on read; h264parse otherwise defaults to AVC/length-
    // prefixed output, which multifilesrc->decodebin cannot detect.
    GstCaps* bs = gst_caps_new_simple(
        isH264 ? "video/x-h264" : "video/x-h265",
        "stream-format", G_TYPE_STRING, "byte-stream",
        "alignment", G_TYPE_STRING, "au", nullptr);
    const gboolean tail = gst_element_link_filtered(parse, sink, bs);
    gst_caps_unref(bs);
    if (!gst_element_link_many(src, conv, caps, enc, parse, nullptr) || !tail) {
        std::cerr << "Failed to link the encode pipeline.\n";
        gst_object_unref(pipe);
        return false;
    }

    gst_element_set_state(pipe, GST_STATE_PLAYING);
    GstBus* bus = gst_element_get_bus(pipe);
    GstMessage* msg = gst_bus_timed_pop_filtered(
        bus, GST_CLOCK_TIME_NONE,
        static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
    bool ok = false;
    if (msg) {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
            ok = true;
        } else {
            GError* err = nullptr; gchar* dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            std::cerr << "Encode error: " << (err ? err->message : "?") << "\n";
            if (dbg) std::cerr << "  debug: " << dbg << "\n";
            g_clear_error(&err); g_free(dbg);
        }
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
    gst_element_set_state(pipe, GST_STATE_NULL);
    gst_object_unref(pipe);
    return ok;
}

// Lower the RTSP jitter buffer and prefer TCP (more reliable through NAT/GRE).
void OnSourceSetup(GstElement* /*bin*/, GstElement* source, gpointer /*data*/) {
    GObjectClass* klass = G_OBJECT_GET_CLASS(source);
    if (g_object_class_find_property(klass, "latency"))
        g_object_set(source, "latency", static_cast<guint>(150), nullptr);
    if (g_object_class_find_property(klass, "protocols"))
        g_object_set(source, "protocols", 0x4 /* GST_RTSP_LOWER_TRANS_TCP */, nullptr);
}

// Find the active decoder factory name inside a decodebin (d3d11h265dec,
// d3d12h264dec, nvh264dec, qsv*, or software avdec_*) so we can confirm
// per-tile whether hardware decode was chosen.
std::string FindDecoderName(GstBin* bin) {
    std::string found;
    GstIterator* it = gst_bin_iterate_recurse(bin);
    GValue item = G_VALUE_INIT;
    gboolean done = FALSE;
    while (!done) {
        switch (gst_iterator_next(it, &item)) {
            case GST_ITERATOR_OK: {
                GstElement* e = GST_ELEMENT(g_value_get_object(&item));
                GstElementFactory* f = gst_element_get_factory(e);
                if (f) {
                    const gchar* name = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(f));
                    if (name && g_strrstr(name, "dec") &&
                        (g_strrstr(name, "d3d11") || g_strrstr(name, "d3d12") ||
                         g_strrstr(name, "nv") || g_strrstr(name, "qsv") ||
                         g_strrstr(name, "avdec") || g_strrstr(name, "va"))) {
                        found = name;
                    }
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
    return found.empty() ? "(unknown)" : found;
}

// Counts decoded frames for this branch. Runs on the streaming thread.
GstPadProbeReturn CountProbe(GstPad* /*pad*/, GstPadProbeInfo* /*info*/, gpointer user_data) {
    static_cast<Branch*>(user_data)->buffers.fetch_add(1, std::memory_order_relaxed);
    return GST_PAD_PROBE_OK;
}

// uridecodebin exposes its decoded pad dynamically; link the first video pad
// into this branch's queue and attach the frame-counting probe.
void OnPadAdded(GstElement* dbin, GstPad* pad, gpointer user_data) {
    auto* b = static_cast<Branch*>(user_data);
    if (b->linked) return;

    GstCaps* caps = gst_pad_get_current_caps(pad);
    if (!caps) caps = gst_pad_query_caps(pad, nullptr);
    bool isVideo = false;
    if (caps) {
        const GstStructure* st = gst_caps_get_structure(caps, 0);
        const gchar* n = gst_structure_get_name(st);
        isVideo = n && g_str_has_prefix(n, "video/");
        gst_caps_unref(caps);
    }
    if (!isVideo) return;

    GstPad* qsink = gst_element_get_static_pad(b->queue, "sink");
    if (gst_pad_link(pad, qsink) != GST_PAD_LINK_OK) {
        std::cerr << "tile " << b->index << ": failed to link decoded pad to queue\n";
        gst_object_unref(qsink);
        return;
    }
    gst_object_unref(qsink);

    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, CountProbe, b, nullptr);
    b->decoder = FindDecoderName(GST_BIN(dbin));
    b->linked = true;
}

gboolean StatsTick(gpointer user_data) {
    auto* s = static_cast<GridState*>(user_data);

    guint64 rendered = 0, dropped = 0;
    g_object_get(s->fpssink, "frames-rendered", &rendered, "frames-dropped", &dropped, nullptr);

    if (!s->firstFrameSeen && rendered > 0) {
        s->firstFrameSeen = true;
        const double ms = (g_get_monotonic_time() - s->playingUs) / 1000.0;
        std::cout << "first composited frame after " << ms << " ms\n";
    }

    std::cout << "--\n";
    for (auto* b : s->branches) {
        const guint64 now = b->buffers.load(std::memory_order_relaxed);
        const guint64 fps = now - b->lastBuffers;
        b->lastBuffers = now;
        std::cout << "  tile " << b->index << "  " << fps << " fps  [" << b->decoder << "]\n";
    }
    std::cout << "  composited: rendered=" << rendered << " dropped=" << dropped;

#ifdef _WIN32
    FILETIME c, e, k, u, nowFt;
    if (GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u)) {
        ULARGE_INTEGER kern, usr, wall;
        kern.LowPart = k.dwLowDateTime;  kern.HighPart = k.dwHighDateTime;
        usr.LowPart = u.dwLowDateTime;   usr.HighPart = u.dwHighDateTime;
        GetSystemTimeAsFileTime(&nowFt);
        wall.LowPart = nowFt.dwLowDateTime; wall.HighPart = nowFt.dwHighDateTime;
        if (s->lastWall.QuadPart != 0) {
            const double busy = double((kern.QuadPart - s->lastKernel.QuadPart) +
                                       (usr.QuadPart - s->lastUser.QuadPart));
            const double span = double(wall.QuadPart - s->lastWall.QuadPart);
            if (span > 0) {
                const double cpu = 100.0 * busy / (span * s->numProcs);
                std::cout << "  cpu=" << static_cast<int>(cpu + 0.5) << "%";
            }
        }
        s->lastKernel = kern; s->lastUser = usr; s->lastWall = wall;
    }
    PROCESS_MEMORY_COUNTERS pmc{};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        std::cout << "  rss=" << (pmc.WorkingSetSize / (1024 * 1024)) << "MB";
#endif
    std::cout << std::endl;
    return TRUE;  // keep firing
}

gboolean StopAfterTimeout(gpointer user_data) {
    auto* s = static_cast<GridState*>(user_data);
    std::cout << "-- time limit reached --\n";
    g_main_loop_quit(s->loop);
    return FALSE;
}

// Walk up to the element that is a direct child of the pipeline (the per-tile
// uridecodebin), so a deep error (e.g. from the internal rtspsrc) can be
// attributed to its tile.
GstObject* FindTopLevel(GstObject* src, GstElement* pipeline) {
    GstObject* o = src;
    while (o) {
        GstObject* p = GST_OBJECT_PARENT(o);
        if (p == nullptr || p == GST_OBJECT(pipeline)) return o;
        o = p;
    }
    return nullptr;
}

gboolean BusCb(GstBus* /*bus*/, GstMessage* msg, gpointer user_data) {
    auto* s = static_cast<GridState*>(user_data);
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr; gchar* dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);

            // Fatal only if the compositor or sink failed. Any source-side error
            // (a refused/dead RTSP source, its queue or upload) drops just that
            // tile and the rest keep measuring. We do NOT tear the branch down
            // from here — changing its state mid-callback breaks its parent chain
            // and makes a racing second error look fatal.
            GstObject* top = FindTopLevel(GST_MESSAGE_SRC(msg), s->pipeline);
            const bool fatal = (top == GST_OBJECT(s->comp) || top == GST_OBJECT(s->fpssink));
            if (fatal) {
                std::cerr << "FATAL (" << (top ? GST_OBJECT_NAME(top) : "?") << "): "
                          << (err ? err->message : "?") << "\n";
                if (dbg) std::cerr << "  debug: " << dbg << "\n";
                g_main_loop_quit(s->loop);
            } else {
                Branch* hit = nullptr;
                for (auto* b : s->branches)
                    if (top == GST_OBJECT(b->decodebin) || top == GST_OBJECT(b->queue) ||
                        top == GST_OBJECT(b->upload) ||
                        (b->source && top == GST_OBJECT(b->source))) { hit = b; break; }
                if (hit && !hit->failed) {
                    hit->failed = true;
                    hit->decoder = std::string("(failed: ") + (err ? err->message : "?") + ")";
                    std::cerr << "tile " << hit->index << " dropped: "
                              << (err ? err->message : "?") << "\n";
                } else if (!hit) {
                    std::cerr << "source error (unattributed): "
                              << (err ? err->message : "?") << "\n";
                }
                int failedCount = 0;
                for (auto* b : s->branches) if (b->failed) ++failedCount;
                if (failedCount == static_cast<int>(s->branches.size())) {
                    std::cerr << "all tiles failed; stopping.\n";
                    g_main_loop_quit(s->loop);
                }
            }
            g_clear_error(&err); g_free(dbg);
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
                if (newS == GST_STATE_PLAYING && s->playingUs == 0)
                    s->playingUs = g_get_monotonic_time();
            }
            break;
        default: break;
    }
    return TRUE;
}

}  // namespace

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    std::vector<std::string> urls;
    int count = 0;      // if >0, replicate the single URL this many times
    int seconds = 30;
    int outW = 1600, outH = 900;
    bool testPattern = false;   // camera-free synthetic fan-out
    std::string codec = "h265"; // test-pattern codec
    int srcW = 1920, srcH = 1080;  // test-pattern source resolution
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--count" && i + 1 < argc)        count = std::atoi(argv[++i]);
        else if (a == "--seconds" && i + 1 < argc) seconds = std::atoi(argv[++i]);
        else if (a == "--width" && i + 1 < argc)   outW = std::atoi(argv[++i]);
        else if (a == "--height" && i + 1 < argc)  outH = std::atoi(argv[++i]);
        else if (a == "--test-pattern")            testPattern = true;
        else if (a == "--codec" && i + 1 < argc)   codec = argv[++i];
        else if (a == "--srcw" && i + 1 < argc)    srcW = std::atoi(argv[++i]);
        else if (a == "--srch" && i + 1 < argc)    srcH = std::atoi(argv[++i]);
        else if (a.rfind("--", 0) != 0)            urls.push_back(a);
    }

    int n = 0;
    std::string patternFile, encoderUsed;
    if (testPattern) {
        if (codec != "h264" && codec != "h265") {
            std::cerr << "--codec must be h264 or h265\n";
            return 2;
        }
        n = (count > 0) ? count : 4;
    } else {
        if (urls.empty()) {
            if (const char* e = std::getenv("VMS_RTSP_URL")) urls.emplace_back(e);
        }
        if (urls.empty()) {
            std::cerr << "Usage: vms_grid \"rtsp://u:p%40host:port/path\" [more urls...] "
                         "[--count N] [--seconds N] [--width W] [--height H]\n"
                         "       (or set VMS_RTSP_URL). Encode any '@' in the password as %40.\n"
                         "   or: vms_grid --test-pattern --count N [--codec h265|h264] "
                         "[--srcw W] [--srch H] [--seconds N]\n";
            return 2;
        }
        if (count > 0) {
            const std::string base = urls.front();
            urls.assign(count, base);
        }
        n = static_cast<int>(urls.size());
    }

    // Square-ish grid geometry.
    const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
    const int rows = static_cast<int>(std::ceil(static_cast<double>(n) / cols));
    const int tileW = outW / cols;
    const int tileH = outH / rows;
    std::cout << "grid: " << n << " tiles in " << cols << "x" << rows
              << " (" << tileW << "x" << tileH << " each) on a " << outW << "x" << outH
              << " surface\n";

    if (testPattern) {
        patternFile = std::string(g_get_tmp_dir()) + "/vms_grid_pattern." + codec;
        const int frames = (seconds > 0) ? (seconds + 3) * 25 : 250;
        std::cout << "encoding " << codec << " test clip (" << srcW << "x" << srcH
                  << ", " << frames << " frames) -> " << patternFile << " ...\n";
        if (!EncodePattern(codec, srcW, srcH, frames, patternFile, encoderUsed)) {
            std::cerr << "test-pattern encode failed; aborting.\n";
            return 5;
        }
        std::cout << "encoded with " << encoderUsed << "; fanning out " << n
                  << " decode-only branch(es). Sink sync is OFF, so per-tile fps is\n"
                     "max decode throughput; sum(fps)/25 ~= sustainable 25 fps streams.\n";
    }

    GridState s;
    s.loop = g_main_loop_new(nullptr, FALSE);
#ifdef _WIN32
    SYSTEM_INFO si; GetSystemInfo(&si); s.numProcs = si.dwNumberOfProcessors;
#endif

    s.pipeline = gst_pipeline_new("grid");
    GstElement* comp = gst_element_factory_make("d3d11compositor", "mix");
    s.fpssink = gst_element_factory_make("fpsdisplaysink", "fps");
    GstElement* sink = gst_element_factory_make("d3d11videosink", "sink");
    if (!s.pipeline || !comp || !s.fpssink || !sink) {
        std::cerr << "Failed to create core elements (need the GStreamer d3d11 plugin "
                     "with d3d11compositor + d3d11videosink, and fpsdisplaysink).\n";
        return 3;
    }
    s.comp = comp;
    // In test-pattern mode let decode run unthrottled (measure max throughput);
    // for real RTSP keep the sink synced so fps reflects the true stream rate.
    if (testPattern) g_object_set(sink, "sync", FALSE, nullptr);
    g_object_set(s.fpssink, "video-sink", sink, "text-overlay", FALSE, nullptr);
    gst_bin_add_many(GST_BIN(s.pipeline), comp, s.fpssink, nullptr);
    if (!gst_element_link(comp, s.fpssink)) {
        std::cerr << "Failed to link compositor -> sink\n";
        return 3;
    }

    for (int i = 0; i < n; ++i) {
        auto* b = new Branch();
        b->index = i;
        b->queue = gst_element_factory_make("queue", nullptr);
        b->upload = gst_element_factory_make("d3d11upload", nullptr);
        if (!b->queue || !b->upload) {
            std::cerr << "Failed to create branch elements for tile " << i << "\n";
            return 3;
        }
        // Drop stale frames on a slow branch rather than stalling the compositor.
        g_object_set(b->queue, "leaky", 2 /*downstream*/, "max-size-buffers", 3,
                     "max-size-time", static_cast<guint64>(0), "max-size-bytes", 0, nullptr);

        if (testPattern) {
            // Camera-free: loop the pre-encoded clip through decodebin so it
            // picks the same hardware decoder rank a real stream would.
            b->source = gst_element_factory_make("multifilesrc", nullptr);
            b->decodebin = gst_element_factory_make("decodebin", nullptr);
            if (!b->source || !b->decodebin) {
                std::cerr << "Failed to create test-pattern elements for tile " << i << "\n";
                return 3;
            }
            g_object_set(b->source, "location", patternFile.c_str(), nullptr);
            if (g_object_class_find_property(G_OBJECT_GET_CLASS(b->source), "loop"))
                g_object_set(b->source, "loop", TRUE, nullptr);
            g_signal_connect(b->decodebin, "pad-added", G_CALLBACK(OnPadAdded), b);
            gst_bin_add_many(GST_BIN(s.pipeline), b->source, b->decodebin,
                             b->queue, b->upload, nullptr);
            if (!gst_element_link(b->source, b->decodebin)) {
                std::cerr << "Failed to link source -> decodebin for tile " << i << "\n";
                return 3;
            }
        } else {
            b->decodebin = gst_element_factory_make("uridecodebin", nullptr);
            if (!b->decodebin) {
                std::cerr << "Failed to create uridecodebin for tile " << i << "\n";
                return 3;
            }
            g_object_set(b->decodebin, "uri", urls[i].c_str(), nullptr);
            g_signal_connect(b->decodebin, "source-setup", G_CALLBACK(OnSourceSetup), nullptr);
            g_signal_connect(b->decodebin, "pad-added", G_CALLBACK(OnPadAdded), b);
            gst_bin_add_many(GST_BIN(s.pipeline), b->queue, b->upload, b->decodebin, nullptr);
        }

        if (!gst_element_link(b->queue, b->upload)) {
            std::cerr << "Failed to link queue -> upload for tile " << i << "\n";
            return 3;
        }

        // Request a compositor sink pad and place this tile in the grid.
        b->compPad = gst_element_request_pad_simple(comp, "sink_%u");
        const int cx = (i % cols) * tileW;
        const int cy = (i / cols) * tileH;
        g_object_set(b->compPad, "xpos", cx, "ypos", cy, "width", tileW, "height", tileH, nullptr);

        GstPad* upSrc = gst_element_get_static_pad(b->upload, "src");
        if (gst_pad_link(upSrc, b->compPad) != GST_PAD_LINK_OK) {
            std::cerr << "Failed to link upload -> compositor pad for tile " << i << "\n";
            gst_object_unref(upSrc);
            return 3;
        }
        gst_object_unref(upSrc);
        s.branches.push_back(b);
    }

    GstBus* bus = gst_element_get_bus(s.pipeline);
    gst_bus_add_watch(bus, BusCb, &s);
    gst_object_unref(bus);

    std::cout << "connecting to " << n << " source(s) (credentials not shown)...\n";
    if (gst_element_set_state(s.pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Failed to set pipeline to PLAYING\n";
        return 4;
    }

    g_timeout_add_seconds(1, StatsTick, &s);
    if (seconds > 0) g_timeout_add_seconds(seconds, StopAfterTimeout, &s);

    g_main_loop_run(s.loop);

    guint64 rendered = 0, dropped = 0;
    g_object_get(s.fpssink, "frames-rendered", &rendered, "frames-dropped", &dropped, nullptr);
    std::cout << "SUMMARY: tiles=" << n << " composited rendered=" << rendered
              << " dropped=" << dropped << std::endl;

    if (testPattern) {
        guint64 totalFrames = 0;
        for (auto* b : s.branches)
            totalFrames += b->buffers.load(std::memory_order_relaxed);
        const double elapsed =
            s.playingUs ? (g_get_monotonic_time() - s.playingUs) / 1e6 : 0.0;
        if (elapsed > 0) {
            const double aggFps = totalFrames / elapsed;
            std::cout << "SUMMARY(test-pattern): codec=" << codec << " src=" << srcW
                      << "x" << srcH << " tiles=" << n
                      << " aggregate decode fps=" << static_cast<int>(aggFps + 0.5)
                      << "  (~" << static_cast<int>(aggFps / 25.0 + 0.5)
                      << " sustainable 25fps streams)" << std::endl;
        }
    }

    gst_element_set_state(s.pipeline, GST_STATE_NULL);
    gst_object_unref(s.pipeline);
    g_main_loop_unref(s.loop);
    return 0;
}
