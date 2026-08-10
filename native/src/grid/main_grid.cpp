// Increment 4 — multi-tile grid decode/render measurement + governor harness.
//
// Decodes N RTSP streams (each preferring a hardware d3d11/d3d12 decoder when
// available), composites them into one grid on a single Direct3D 11 surface,
// and reports per-tile decode fps, the decoder each tile selected, aggregate
// frames rendered/dropped, and this process's CPU% and working set.
//
// Without --govern this is the 4a MEASUREMENT spike: it applies no priority or
// tier policy and finds the smooth ceiling per hardware tier. With --govern it
// is the 4b integration harness: the startup hardware probe seeds a conservative
// capacity profile and the governor maps requested Main tiles to cheaper tiers.
//
// Usage:
//   vms_grid "rtsp://u:p%40host:port/path" [more urls...] [--count N]
//            [--seconds N] [--width W] [--height H]
//            [--rtsp-transport auto|tcp|udp|multicast]
//   --count N   replicate the single provided URL N times (ramp to find ceiling)
//   --seconds N run time; 0 = until the window/console is closed (default 30)
//   (or set VMS_RTSP_URL for a single source)
//
// Governed live cameras (the governor selects each tile's real stream by tier):
//   vms_grid --govern [--sweep] --camera "rtsp://MAIN;rtsp://SUB" [--camera ...]
//            [--count N] [--profile auto|devbox|lowend] [--seconds N]
//   Main tier opens the main stream, Sub/Thumb the sub stream, Paused opens no
//   session. Pass one --camera per camera as "mainUrl;subUrl" (sub optional).
//   URLs are never printed — only a credential-free "stream selection:" line.
//
// Camera-free fan-out (to measure the PURE decode+composite ceiling of a
// machine, without a camera's session cap or a single low-res sub-stream):
//   vms_grid --test-pattern --count N [--codec h265|h264]
//            [--srcw W] [--srch H] [--seconds N]
//            [--govern] [--profile auto|devbox|lowend]
//   Encodes one synthetic clip up front, then fans out N decode-only branches
//   from it. The video sink runs with sync OFF, so each tile decodes as fast
//   as the hardware allows: per-tile fps is the max decode throughput, and
//   sum(fps)/25 estimates how many realtime 25 fps streams this box sustains.
//   Use --codec h264 on machines with no H.265 hardware decode (e.g. the
//   low-end i5 tier) to measure their QuickSync/DXVA H.264 ceiling instead.
//   --profile defaults to auto; named profiles are reproducible test overrides.
//
// Dynamic re-planning (the 4b "apply the plan to live branches" step):
//   vms_grid --test-pattern --govern --sweep [--sweep-interval N] --count 64 ...
//   Builds every requested tile as a live branch at its governed tier, then every
//   N seconds (default 4) moves the focused tile to the next cell, re-runs the
//   stateful governor, diffs the new plan against the running one, and applies
//   each per-tile change to the LIVE pipeline: an upgraded/downgraded tile swaps
//   its decoded source resolution in place, a paused tile releases its decoder
//   (shown as a black, non-live tile), and a resumed tile rebuilds one. Releases
//   are applied before acquisitions so the machine is never transiently
//   over-subscribed. This exercises degrade/recover with hysteresis on real
//   decode branches instead of a one-shot startup plan.
//
// Credentials are supplied at run time only and are never stored or committed.

#include <gst/gst.h>

#include "governor/Governor.h"
#include "media/GstRtspPolicy.h"
#ifdef _WIN32
#include "hardware/HardwareProbe.h"
#endif

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

#ifdef VMS_WITH_BROKER
#include "broker/ConnectionBroker.h"
#include "persist/CredentialRepo.h"
#include "persist/Schema.h"
#include "persist/SecretStore.h"
#include "persist/Store.h"
#endif

namespace {

const char* PickFactory(const char* const* names);

#ifdef _WIN32
vms::CapacityProfile AutoCapacityProfile(const std::string& codec) {
    const vms::MachineProfile machine = vms::ProbeMachine();
    const vms::Codec workingCodec = (codec == "h264") ? vms::Codec::H264 : vms::Codec::H265;
    const char* const h264HardwareDecoders[] = {
        "d3d12h264dec", "d3d11h264dec", "nvh264dec", "qsvh264dec", nullptr
    };
    const char* const h265HardwareDecoders[] = {
        "d3d12h265dec", "d3d11h265dec", "nvh265dec", "qsvh265dec", nullptr
    };
    const bool hasRegisteredHardwareDecoder =
        PickFactory(workingCodec == vms::Codec::H264
                        ? h264HardwareDecoders
                        : h265HardwareDecoders) != nullptr;

    vms::HardwareInputs hw;
    hw.totalRamMb =
        static_cast<double>(machine.totalRamBytes) / (1024.0 * 1024.0);
    hw.logicalCores = machine.logicalCores;

    std::string decoderAdapter;
    for (const auto& gpu : machine.gpus) {
        if (gpu.isSoftwareAdapter) continue;
        const bool supports =
            (workingCodec == vms::Codec::H264) ? gpu.decode.h264 : gpu.decode.h265Main;
        if (!supports || !hasRegisteredHardwareDecoder) continue;

        hw.hasHardwareDecode = true;
        const double dedicatedMb =
            static_cast<double>(gpu.dedicatedVideoMemoryBytes) / (1024.0 * 1024.0);
        // Small "dedicated" allocations generally describe integrated/shared
        // graphics. HardwareInputs intentionally represents those as unknown.
        if (dedicatedMb >= 512.0 && dedicatedMb > hw.videoMemoryMb) {
            hw.videoMemoryMb = dedicatedMb;
            decoderAdapter = gpu.description;
        } else if (decoderAdapter.empty()) {
            decoderAdapter = gpu.description;
        }
    }

    std::ostringstream label;
    label << "auto " << codec << " (" << machine.tier << ", "
          << (hw.hasHardwareDecode ? "hardware" : "software") << " decode";
    if (!decoderAdapter.empty()) label << ", " << decoderAdapter;
    label << ")";
    return vms::MakeCapacityProfile(hw, label.str());
}
#endif

// One camera's two stream URLs. The governor's tier decision selects which one a
// tile opens: Main -> the full-resolution main stream; Sub/Thumb -> the lower-
// resolution sub stream (a camera typically publishes exactly these two); Paused
// -> nothing is opened, so an off-working-set camera holds no session. These are
// operator-provided URLs; discovering a device's profiles (ONVIF/P2-05) is a
// separate, later gate and is deliberately not done here.
struct Camera {
    std::string main;
    std::string sub;   // empty -> Sub/Thumb fall back to the main stream
};

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
    vms::Tier tier = vms::Tier::Main;  // current governed tier of this branch
    vms::TileState state = vms::TileState::Live;  // honest governed state
};

struct GridState {
    GstElement* pipeline = nullptr;
    GstElement* comp = nullptr;
    GstElement* fpssink = nullptr;
    GMainLoop* loop = nullptr;
    std::vector<Branch*> branches;
    gint64 playingUs = 0;
    bool firstFrameSeen = false;

    // Dynamic re-planning state (--sweep). Populated only when governing live.
    bool sweep = false;
    int sweepIntervalSec = 4;
    int focusIndex = 0;
    vms::GovernorSession* session = nullptr;   // owned; deleted after the loop
    std::vector<vms::TileRequest> reqs;         // one entry per branch, id == index
    vms::GovernorResult plan;                   // the plan currently applied
    std::string tierFile[4];                    // clip path per (int)Tier: 1=thumb 2=sub 3=main

    // Governed-RTSP state: real per-camera streams selected by tier (see Camera).
    bool rtsp = false;                           // build fronts from cameras, not clips
    std::vector<Camera> cameras;                 // one entry per branch, id == index
    vms::media::RtspTransportPolicy rtspPolicy;
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
        "framerate", GST_TYPE_FRACTION, 25, 1,
        // Camera H.264/H.265 main profiles are normally 4:2:0. Without an
        // explicit format x264 can negotiate 4:4:4, which hardware decoders do
        // not accept and makes decodebin fall back to software.
        "format", G_TYPE_STRING, "I420", nullptr);
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

void OnSourceSetup(GstElement* /*bin*/, GstElement* source, gpointer data) {
    auto* state = static_cast<GridState*>(data);
    if (state) vms::media::ApplyRtspTransportPolicy(source, state->rtspPolicy);
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
        std::cout << "  tile " << b->index << "  [" << vms::TierName(b->tier) << "/"
                  << vms::TileStateName(b->state) << "]  " << fps << " fps  ["
                  << b->decoder << "]\n";
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

// ---- Dynamic re-planning: apply a changed governor plan to live branches ----
//
// Each branch has a stable spine (queue -> upload -> compositor pad) that never
// moves. Only the decode "front" (source [+ decodebin]) is swapped when a tile's
// tier changes, so the compositor is never disturbed and other tiles keep
// running. All of this runs on the main-loop thread between StatsTick calls.

// Detach and destroy whatever currently feeds this branch's queue.
void TeardownBranchFront(GridState* s, Branch* b) {
    GstPad* qsink = gst_element_get_static_pad(b->queue, "sink");
    if (GstPad* peer = gst_pad_get_peer(qsink)) {
        gst_pad_unlink(peer, qsink);
        gst_object_unref(peer);
    }
    gst_object_unref(qsink);
    if (b->decodebin) {
        gst_element_set_state(b->decodebin, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(s->pipeline), b->decodebin);  // transfers the ref
        b->decodebin = nullptr;
    }
    if (b->source) {
        gst_element_set_state(b->source, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(s->pipeline), b->source);
        b->source = nullptr;
    }
    b->linked = false;
}

// The stream URL a tier selects for a camera: Main -> main stream; Sub and Thumb
// -> the sub stream (a camera usually has just those two), falling back to main
// when no sub URL was provided. Paused opens nothing and is handled before this.
const std::string& UriForTier(const Camera& c, vms::Tier tier) {
    if (tier == vms::Tier::Main || c.sub.empty()) return c.main;
    return c.sub;
}

// A concise, credential-free name for the stream a tier selects (never print the
// URL itself — it carries the password).
const char* StreamNameForTier(vms::Tier tier) {
    switch (tier) {
        case vms::Tier::Main:  return "main";
        case vms::Tier::Sub:   return "sub";
        case vms::Tier::Thumb: return "sub(thumb)";
        case vms::Tier::Paused: return "none";
    }
    return "?";
}

// Build the decode front for a tier and link it to the queue. Paused tiles get
// NO decoder (and, for RTSP, open NO session): a cheap black source keeps the
// compositor pad fed while honestly showing no live video. In --test-pattern
// mode the front is a looped synthetic clip; in governed-RTSP mode it is a
// uridecodebin on the tier's real stream URL. Assumes the queue is already in
// the pipeline; the caller syncs element state when the pipeline is running.
bool BuildBranchFront(GridState* s, Branch* b, vms::Tier tier) {
    b->tier = tier;
    b->linked = false;

    if (tier == vms::Tier::Paused) {
        b->source = gst_element_factory_make("videotestsrc", nullptr);
        b->decodebin = nullptr;
        if (!b->source) return false;
        // pattern 2 = solid black; is-live paces it so it does not flood the
        // sync-off sink. No decoder is instantiated for a paused tile.
        g_object_set(b->source, "pattern", 2, "is-live", TRUE, nullptr);
        gst_bin_add(GST_BIN(s->pipeline), b->source);
        GstPad* vsrc = gst_element_get_static_pad(b->source, "src");
        GstPad* qsink = gst_element_get_static_pad(b->queue, "sink");
        const bool ok = gst_pad_link(vsrc, qsink) == GST_PAD_LINK_OK;
        gst_object_unref(vsrc);
        gst_object_unref(qsink);
        b->decoder = "(paused)";
        b->linked = ok;
        return ok;
    }

    if (s->rtsp) {
        // Governed live camera: open the stream this tier selected. uridecodebin
        // is source + demux + decoder in one; it exposes its decoded pad
        // dynamically (OnPadAdded links it to the queue), so there is no separate
        // source element to link here.
        b->source = nullptr;
        b->decodebin = gst_element_factory_make("uridecodebin", nullptr);
        if (!b->decodebin) return false;
        const std::string& uri = UriForTier(s->cameras[b->index], tier);
        g_object_set(b->decodebin, "uri", uri.c_str(), nullptr);
        g_signal_connect(b->decodebin, "source-setup", G_CALLBACK(OnSourceSetup), s);
        g_signal_connect(b->decodebin, "pad-added", G_CALLBACK(OnPadAdded), b);
        gst_bin_add(GST_BIN(s->pipeline), b->decodebin);
        b->decoder = "(pending)";
        return true;
    }

    b->source = gst_element_factory_make("multifilesrc", nullptr);
    b->decodebin = gst_element_factory_make("decodebin", nullptr);
    if (!b->source || !b->decodebin) return false;
    g_object_set(b->source, "location", s->tierFile[static_cast<int>(tier)].c_str(), nullptr);
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(b->source), "loop"))
        g_object_set(b->source, "loop", TRUE, nullptr);
    g_signal_connect(b->decodebin, "pad-added", G_CALLBACK(OnPadAdded), b);
    gst_bin_add_many(GST_BIN(s->pipeline), b->source, b->decodebin, nullptr);
    b->decoder = "(pending)";
    return gst_element_link(b->source, b->decodebin) != FALSE;
}

// Every interval: move focus to the next tile, re-run the stateful governor,
// diff against the applied plan, and — only when the plan actually changed —
// reconfigure the grid. An in-place hot-swap into a running pipeline was tried
// first and does not survive the D3D12 hardware decoder plus the reused
// compositor pad (the rebuilt decoder wedges in async PAUSED at 0 fps and stalls
// teardown). The reliable mechanism is the one the flawless startup uses: take
// the whole graph to NULL, rebuild EVERY branch's decode front fresh at its new
// tier, and go PLAYING so they all preroll together from zero. (Cycling a front
// that was itself rebuilt in a prior swap without rebuilding it fails with an
// internal-stream error, so all fronts are rebuilt uniformly each time.)
gboolean SweepTick(gpointer user_data) {
    auto* s = static_cast<GridState*>(user_data);
    if (!s->session || s->reqs.empty()) return TRUE;

    s->focusIndex = (s->focusIndex + 1) % static_cast<int>(s->reqs.size());
    for (auto& r : s->reqs) { r.focused = false; r.priority = vms::Priority::Medium; }
    s->reqs[s->focusIndex].focused = true;
    s->reqs[s->focusIndex].priority = vms::Priority::High;

    const vms::GovernorResult next = s->session->update(s->reqs);
    const std::vector<vms::TileTransition> moves = vms::DiffPlans(s->plan, next);

    std::cout << "== sweep: focus -> tile " << s->focusIndex << "; "
              << moves.size() << " live transition(s) ==\n";
    for (const auto& t : moves)
        std::cout << "  tile " << t.id << ": " << vms::TierName(t.from) << " -> "
                  << vms::TierName(t.to) << (t.release() ? "  (release)" : "  (acquire)")
                  << "\n";

    if (!moves.empty()) {
        gst_element_set_state(s->pipeline, GST_STATE_NULL);
        gst_element_get_state(s->pipeline, nullptr, nullptr, GST_CLOCK_TIME_NONE);

        for (Branch* b : s->branches) {
            vms::Tier target = vms::Tier::Paused;
            vms::TileState state = vms::TileState::PausedOffscreen;
            for (const auto& d : next.tiles)
                if (d.id == b->index) { target = d.tier; state = d.state; break; }
            TeardownBranchFront(s, b);
            b->failed = false;
            if (!BuildBranchFront(s, b, target)) {
                std::cerr << "tile " << b->index << ": rebuild at "
                          << vms::TierName(target) << " failed\n";
                b->failed = true;
            }
            b->state = state;
        }

        gst_element_set_state(s->pipeline, GST_STATE_PLAYING);
    }
    s->plan = next;
    return TRUE;
}

}  // namespace

#ifdef VMS_WITH_BROKER
// ---- Native increment 6c-2: connect through the ConnectionBroker by camera_id.
// The run path takes camera_ids (never a URL-with-password); the broker resolves
// each camera's device + credential-free URL template + secret and materializes
// the credentialed URL in memory. We resolve Main/Sub once into the existing
// Camera{main,sub} vector, so the entire branch/replan/teardown pipeline below is
// reused unchanged — only the SOURCE of the URLs moved behind the broker.
namespace {

using vms::broker::ConnectionBroker;
using vms::broker::Session;
using vms::broker::StreamKind;
using vms::persist::CredentialRepo;
using vms::persist::Error;
using vms::persist::InMemorySecretStore;
using vms::persist::Store;
using vms::persist::Value;

// "<camera_id>|<user:pass>|<mainTemplate>[|<subTemplate>]"
struct ProvisionSpec {
    std::string cameraId, secret, mainUrl, subUrl;
    bool ok = false;
};
ProvisionSpec ParseProvision(const std::string& spec) {
    ProvisionSpec p;
    std::vector<std::string> parts;
    std::string cur;
    std::istringstream ss(spec);
    while (std::getline(ss, cur, '|')) parts.push_back(cur);
    if (parts.size() >= 3) {
        p.cameraId = parts[0];
        p.secret = parts[1];
        p.mainUrl = parts[2];
        if (parts.size() >= 4) p.subUrl = parts[3];
        p.ok = !p.cameraId.empty() && !p.mainUrl.empty();
    }
    return p;
}

// Write one camera's device + credential-free URL templates + encrypted secret.
// device_id is derived from the camera_id; the secret goes through the
// CredentialRepo so no plaintext reaches SQLite.
Error ProvisionCamera(Store& store, CredentialRepo& creds, const ProvisionSpec& p) {
    const std::string dev = p.cameraId + "-dev";
    if (Error e = store.exec(
            "INSERT INTO devices(id, name) VALUES(?, ?) "
            "ON CONFLICT(id) DO NOTHING;", {dev, p.cameraId}); !e)
        return e;
    const Value subVal = p.subUrl.empty() ? Value{nullptr} : Value{p.subUrl};
    if (Error e = store.exec(
            "INSERT INTO cameras(id, device_id, name, main_url, sub_url) "
            "VALUES(?, ?, ?, ?, ?) ON CONFLICT(id) DO UPDATE SET "
            "device_id=excluded.device_id, main_url=excluded.main_url, "
            "sub_url=excluded.sub_url;",
            {p.cameraId, dev, p.cameraId, p.mainUrl, subVal}); !e)
        return e;
    return creds.put(dev, p.secret);
}

// Resolve each camera_id's Main and Sub stream URLs through the broker, filling
// the Camera{main,sub} the pipeline consumes. Sub is optional (falls back to
// main). Sessions are released after materialization (we only need the URL).
bool ResolveCamerasViaBroker(ConnectionBroker& broker,
                             const std::vector<std::string>& ids,
                             std::vector<Camera>& out) {
    for (const std::string& id : ids) {
        Camera cam;
        Session s;
        if (Error e = broker.connect(id, StreamKind::Main, s); !e) {
            std::cerr << "broker: cannot open main stream for '" << id
                      << "': " << e.message << "\n";
            return false;
        }
        cam.main = s.url;
        broker.release(s);
        Session sub;
        if (Error e = broker.connect(id, StreamKind::Sub, sub); e) {
            cam.sub = sub.url;
            broker.release(sub);
        }
        out.push_back(cam);
    }
    return true;
}

// Build cameras for the run entirely from the broker: open the DB, migrate,
// apply any provisioning, then resolve the requested camera_ids. All secret and
// URL handling stays behind the broker; the caller only ever named camera_ids.
bool PrepareCamerasFromBroker(const std::string& dbPath,
                              const std::vector<std::string>& provisionSpecs,
                              std::vector<std::string> cameraIds,
                              std::vector<Camera>& out) {
    Store store;
    if (Error e = store.open(dbPath); !e) {
        std::cerr << "broker: cannot open db '" << dbPath << "': " << e.message << "\n";
        return false;
    }
    if (Error e = store.migrate(vms::persist::coreMigrations()); !e) {
        std::cerr << "broker: migrate failed: " << e.message << "\n";
        return false;
    }
    InMemorySecretStore secrets;  // NOTE: for a persistent secret store use DpapiSecretStore
    CredentialRepo creds(store, secrets);
    for (const std::string& spec : provisionSpecs) {
        ProvisionSpec p = ParseProvision(spec);
        if (!p.ok) {
            std::cerr << "broker: bad --provision '" << spec
                      << "' (want \"id|user:pass|mainUrl[|subUrl]\")\n";
            return false;
        }
        if (Error e = ProvisionCamera(store, creds, p); !e) {
            std::cerr << "broker: provision '" << p.cameraId << "' failed: "
                      << e.message << "\n";
            return false;
        }
        if (cameraIds.empty()) cameraIds.push_back(p.cameraId);  // default: run what we provisioned
    }
    ConnectionBroker broker(store, creds);
    return ResolveCamerasViaBroker(broker, cameraIds, out);
}

// Headless proof that vms_grid connects through the broker by camera_id (no
// GStreamer, no camera, no window). Provisions a synthetic camera, resolves its
// tiers through the broker, and asserts the credentialed URL is materialized in
// memory while the run input was only a camera_id. Prints a credential-free line.
int RunBrokerSelftest() {
    std::cout << "vms_grid --broker-selftest (connect-by-camera_id via the broker)\n";
    int failures = 0;
    auto check = [&](bool cond, const std::string& what) {
        std::cout << (cond ? "  ok  " : "  FAIL ") << what << "\n";
        if (!cond) ++failures;
    };

    std::vector<std::string> provs = {
        "cam-1|admin:p@ss/w0rd|rtsp://10.0.0.5:554/Streaming/Channels/101"
        "|rtsp://10.0.0.5:554/Streaming/Channels/102"};
    std::vector<Camera> cams;
    const bool ok = PrepareCamerasFromBroker(":memory:", provs, {"cam-1"}, cams);
    check(ok && cams.size() == 1, "resolved 1 camera through the broker");
    if (ok && cams.size() == 1) {
        check(cams[0].main.find('@') != std::string::npos &&
                  cams[0].main.find("%40") != std::string::npos,
              "main URL is materialized with percent-encoded credentials (in memory)");
        check(cams[0].main.find("/Streaming/Channels/101") != std::string::npos,
              "main URL selected the main stream template");
        check(cams[0].sub.find("/Streaming/Channels/102") != std::string::npos,
              "sub URL selected the sub stream template");
        // Credential-free operator-facing line (the URL itself is never printed).
        std::cout << "  stream selection: cam-1 -> main+sub resolved by broker "
                     "(run input was a camera_id, not a URL-with-password)\n";
    }

    // An unknown camera_id fails honestly rather than materializing anything.
    std::vector<Camera> none;
    const bool unknownOk = PrepareCamerasFromBroker(":memory:", {}, {"ghost"}, none);
    check(!unknownOk && none.empty(), "unknown camera_id fails honestly (no URL materialized)");

    if (failures == 0) {
        std::cout << "PASS: vms_grid resolves streams through the broker by camera_id\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}

}  // namespace
#endif  // VMS_WITH_BROKER

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    std::vector<std::string> urls;
    std::vector<Camera> cameras;   // --camera "mainUrl[;subUrl]" (governed RTSP)
    int count = 0;      // if >0, replicate the single URL this many times
    int seconds = 30;
    int outW = 1600, outH = 900;
    bool testPattern = false;   // camera-free synthetic fan-out
    std::string codec = "h265"; // test-pattern codec
    int srcW = 1920, srcH = 1080;  // test-pattern source resolution
    bool govern = false;                  // apply the P3-03 governor to tier the tiles
    std::string profileName = "auto";     // auto | devbox | lowend
    bool sweep = false;                   // re-plan live as focus moves
    int sweepInterval = 4;                // seconds between focus moves
    std::string brokerDb;                 // --broker-db <path>: connect via the broker
    std::vector<std::string> provisionSpecs;  // --provision "id|user:pass|main[|sub]"
    std::vector<std::string> cameraIds;   // --camera-id <id>: which cameras to open (broker)
    bool brokerSelftest = false;          // --broker-selftest: headless integration check
    vms::media::RtspTransportPolicy rtspPolicy;
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
        else if (a == "--govern")                  govern = true;
        else if (a == "--profile" && i + 1 < argc) profileName = argv[++i];
        else if (a == "--sweep")                   sweep = true;
        else if (a == "--sweep-interval" && i + 1 < argc) sweepInterval = std::atoi(argv[++i]);
        else if (a == "--camera" && i + 1 < argc) {
            // "mainUrl[;subUrl]" — the governor selects main vs sub per tile.
            const std::string spec = argv[++i];
            const std::size_t semi = spec.find(';');
            Camera cam;
            cam.main = (semi == std::string::npos) ? spec : spec.substr(0, semi);
            if (semi != std::string::npos) cam.sub = spec.substr(semi + 1);
            cameras.push_back(cam);
        }
        else if (a == "--broker-db" && i + 1 < argc)   brokerDb = argv[++i];
        else if (a == "--provision" && i + 1 < argc)   provisionSpecs.push_back(argv[++i]);
        else if (a == "--camera-id" && i + 1 < argc)   cameraIds.push_back(argv[++i]);
        else if (a == "--broker-selftest")             brokerSelftest = true;
        else if (a == "--rtsp-transport" && i + 1 < argc) {
            if (!vms::media::ParseRtspTransportMode(argv[++i], rtspPolicy.mode)) {
                std::cerr << "--rtsp-transport must be auto, tcp, udp, or multicast\n";
                return 2;
            }
        }
        else if (a.rfind("--", 0) != 0)            urls.push_back(a);
    }

#ifdef VMS_WITH_BROKER
    // Native increment 6c-2: connect through the ConnectionBroker by camera_id.
    if (brokerSelftest) return RunBrokerSelftest();
    if (!brokerDb.empty() || !provisionSpecs.empty() || !cameraIds.empty()) {
        if (brokerDb.empty()) {
            std::cerr << "--provision/--camera-id require --broker-db <path>\n";
            return 2;
        }
        if (!cameras.empty()) {
            std::cerr << "--broker-db resolves cameras by id; do not also pass "
                         "--camera URLs on the same run\n";
            return 2;
        }
        if (!PrepareCamerasFromBroker(brokerDb, provisionSpecs, cameraIds, cameras))
            return 2;
        if (cameras.empty()) {
            std::cerr << "--broker-db: no cameras to open (pass --camera-id or --provision)\n";
            return 2;
        }
    }
#else
    if (brokerSelftest || !brokerDb.empty() || !provisionSpecs.empty() ||
        !cameraIds.empty()) {
        std::cerr << "broker options require the Windows broker build (VMS_WITH_BROKER)\n";
        return 2;
    }
#endif

    int n = 0;
    std::string encoderUsed;
    if (testPattern) {
        if (codec != "h264" && codec != "h265") {
            std::cerr << "--codec must be h264 or h265\n";
            return 2;
        }
        n = (count > 0) ? count : 4;
    } else {
        // RTSP mode. Cameras come from --camera "main[;sub]" (governor selects the
        // stream per tier) or from bare positional URLs (main only). Governed RTSP
        // needs the main/sub split to be meaningful, but works either way.
        if (cameras.empty() && urls.empty()) {
            if (const char* e = std::getenv("VMS_RTSP_URL")) urls.emplace_back(e);
        }
        for (const auto& u : urls) cameras.push_back(Camera{u, ""});
        if (cameras.empty()) {
            std::cerr << "Usage: vms_grid \"rtsp://u:p%40host:port/path\" [more urls...] "
                         "[--count N] [--seconds N] [--width W] [--height H]\n"
                         "       (or set VMS_RTSP_URL). Encode any '@' in the password as %40.\n"
                         "   governed live: vms_grid --govern [--sweep] "
                         "--camera \"rtsp://MAIN;rtsp://SUB\" [--camera ...] [--count N]\n"
                         "   RTSP media: --rtsp-transport auto|tcp|udp|multicast (default auto)\n"
                         "   or: vms_grid --test-pattern --count N [--codec h265|h264] "
                         "[--srcw W] [--srch H] [--seconds N]\n";
            return 2;
        }
        if (count > 0) {
            const Camera base = cameras.front();
            cameras.assign(count, base);
        }
        // urls stays the flat main-only list the non-governed inline path uses.
        urls.clear();
        for (const auto& c : cameras) urls.push_back(c.main);
        n = static_cast<int>(cameras.size());
    }

    const bool governedRtsp = govern && !testPattern;
    if (govern && profileName != "auto" &&
        profileName != "devbox" && profileName != "lowend") {
        std::cerr << "--profile must be auto, devbox, or lowend\n";
        return 2;
    }
    if (sweep && !govern) {
        std::cerr << "--sweep re-plans the governor at run time, so it requires --govern.\n";
        return 2;
    }
    if (governedRtsp) {
        bool anySub = false;
        for (const auto& c : cameras) if (!c.sub.empty()) anySub = true;
        if (!anySub)
            std::cerr << "note: no --camera sub stream given; Sub/Thumb tiers fall back to "
                         "the main stream (no bandwidth saving). Pass \"main;sub\" for both.\n";
    }
    if (sweep && seconds <= sweepInterval) {
        std::cerr << "--sweep needs --seconds greater than --sweep-interval so at least "
                     "one re-plan happens (interval=" << sweepInterval << ").\n";
        return 2;
    }

    // Governor integration. Turns a grid of requested main tiles into a tiered
    // plan that fits the machine's decode + memory budget instead of over-
    // subscribing. In --test-pattern mode a tier selects a synthetic clip; in
    // governed-RTSP mode it selects a camera's real main vs sub stream (Paused
    // opens no session at all).
    std::vector<vms::Tier> tiers;  // per rendered tile (test-pattern / rtsp path)
    std::vector<vms::TileState> tileStates;  // honest per-tile state (full grids)
    // Sweep state, lifted to main scope so the stateful session outlives startup.
    vms::GovernorSession* sweepSession = nullptr;
    std::vector<vms::TileRequest> sweepReqs;
    vms::GovernorResult sweepPlan;
    const bool governing = govern;
    // A full grid keeps every requested tile as a branch (paused ones shown black
    // with a stable id->branch mapping): required for a real camera wall and for
    // run-time re-planning. Only the static test-pattern measurement collapses.
    const bool fullGrid = sweep || governedRtsp;
    if (governing) {
        vms::CapacityProfile prof;
        if (profileName == "devbox") {
            prof = vms::DevBoxProfile();
        } else if (profileName == "lowend") {
            prof = vms::LowEndProfile();
        } else {
#ifdef _WIN32
            prof = AutoCapacityProfile(codec);
#else
            std::cerr << "--profile auto is not implemented on this platform; "
                         "use an explicit calibrated profile.\n";
            return 2;
#endif
        }
        vms::GovernorSession* gov = new vms::GovernorSession(prof);
        std::vector<vms::TileRequest> reqs;
        reqs.reserve(n);
        for (int i = 0; i < n; ++i) {
            vms::TileRequest r;
            r.id = i;
            r.desired = vms::Tier::Main;
            r.priority = (i == 0) ? vms::Priority::High : vms::Priority::Medium;
            r.focused = (i == 0);
            reqs.push_back(r);
        }
        const vms::GovernorResult plan = gov->update(reqs);
        int cm = 0, cs = 0, ct = 0, cp = 0;
        for (const auto& d : plan.tiles) {
            switch (d.tier) {
                case vms::Tier::Main:  ++cm; break;
                case vms::Tier::Sub:   ++cs; break;
                case vms::Tier::Thumb: ++ct; break;
                case vms::Tier::Paused: ++cp; break;
            }
        }
        std::cout << "governor [" << prof.label << "]: requested " << n
                  << " main tiles -> main=" << cm << " sub=" << cs << " thumb=" << ct
                  << " paused=" << cp << "  (model decode=" << plan.decodeUsed
                  << " main-eq, memory=" << plan.memoryUsedMb << "MB)\n";

        if (fullGrid) {
            // Keep EVERY requested tile as a live branch (paused ones included,
            // shown black) so a camera wall has a stable id -> branch mapping and
            // focus can move to any cell. n stays the full requested count.
            tiers.resize(n, vms::Tier::Paused);
            tileStates.assign(n, vms::TileState::PausedOffscreen);
            for (const auto& d : plan.tiles)
                if (d.id >= 0 && d.id < n) { tiers[d.id] = d.tier; tileStates[d.id] = d.state; }
        } else {
            // Static test-pattern measurement: collapse out paused tiles.
            for (const auto& d : plan.tiles)
                if (d.tier != vms::Tier::Paused) tiers.push_back(d.tier);
            n = static_cast<int>(tiers.size());
        }

        if (governedRtsp) {
            // Show which real stream each tile opens (never the URL: it carries
            // the password).
            std::cout << "stream selection: ";
            for (int i = 0; i < n; ++i)
                std::cout << "t" << i << "=" << StreamNameForTier(tiers[i])
                          << (i + 1 < n ? " " : "\n");
        }

        if (sweep) {
            sweepSession = gov;         // ownership passes to the run loop
            sweepReqs = reqs;
            sweepPlan = plan;
        } else {
            delete gov;
        }
        if (!fullGrid && n == 0) {
            std::cerr << "governor paused all tiles; nothing to render.\n";
            return 0;
        }
    } else if (testPattern) {
        tiers.assign(n, vms::Tier::Main);
    }

    // Square-ish grid geometry.
    const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
    const int rows = static_cast<int>(std::ceil(static_cast<double>(n) / cols));
    const int tileW = outW / cols;
    const int tileH = outH / rows;
    std::cout << "grid: " << n << " tiles in " << cols << "x" << rows
              << " (" << tileW << "x" << tileH << " each) on a " << outW << "x" << outH
              << " surface\n";

    // One encoded clip per distinct tier present in the plan (main at the
    // requested source resolution; sub 640x480; thumb 320x240). Indexed by
    // (int)Tier: 1=thumb, 2=sub, 3=main. Encoding is done once, up front, so its
    // cost never pollutes the decode measurement.
    std::string tierFile[4];
    if (testPattern) {
        const int frames = (seconds > 0) ? (seconds + 3) * 25 : 250;
        auto resForTier = [&](vms::Tier t, int& w, int& h) {
            switch (t) {
                case vms::Tier::Sub:   w = 640; h = 480; break;
                case vms::Tier::Thumb: w = 320; h = 240; break;
                default:               w = srcW; h = srcH; break; // Main
            }
        };
        bool need[4] = {false, false, false, false};
        for (vms::Tier t : tiers) need[static_cast<int>(t)] = true;
        // A sweep can drive any tile to any tier at run time, so every decode
        // tier's clip must exist up front.
        if (sweep) { need[1] = need[2] = need[3] = true; }
        for (int ti = 1; ti <= 3; ++ti) {
            if (!need[ti]) continue;
            int w = 0, h = 0;
            resForTier(static_cast<vms::Tier>(ti), w, h);
            const std::string f = std::string(g_get_tmp_dir()) + "/vms_grid_pattern_" +
                                  std::to_string(w) + "x" + std::to_string(h) + "." + codec;
            std::string enc;
            std::cout << "encoding " << codec << " " << w << "x" << h << " clip ("
                      << frames << " frames) -> " << f << " ...\n";
            if (!EncodePattern(codec, w, h, frames, f, enc)) {
                std::cerr << "test-pattern encode failed; aborting.\n";
                return 5;
            }
            tierFile[ti] = f;
            encoderUsed = enc;
        }
        std::cout << "encoded with " << encoderUsed << "; fanning out " << n
                  << " decode branch(es)" << (governing ? " per governor plan" : "")
                  << ". Sink sync is OFF, so per-tile fps is max decode throughput.\n";
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

    // Hand the run loop everything the live re-planner needs.
    for (int ti = 0; ti < 4; ++ti) s.tierFile[ti] = tierFile[ti];
    s.sweep = sweep && governing;
    s.sweepIntervalSec = sweepInterval;
    s.session = sweepSession;        // nullptr unless sweeping; owned here now
    s.reqs = sweepReqs;
    s.plan = sweepPlan;
    s.focusIndex = 0;                // tile 0 is the initially focused tile
    s.rtsp = governedRtsp;           // BuildBranchFront opens real streams by tier
    s.cameras = cameras;             // one entry per branch, id == index
    s.rtspPolicy = rtspPolicy;
    if (!testPattern)
        std::cout << "RTSP lower transport policy: "
                  << vms::media::RtspTransportModeName(s.rtspPolicy.mode) << "\n";

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

        if (s.sweep || (governing && s.rtsp)) {
            // Governed grid (live-replan and/or real cameras): every tile is a
            // branch (paused ones shown black, no decoder / no session). The same
            // BuildBranchFront path used for run-time tier swaps builds the
            // initial front here — synthetic clip or real main/sub stream by tier.
            gst_bin_add_many(GST_BIN(s.pipeline), b->queue, b->upload, nullptr);
            if (!BuildBranchFront(&s, b, tiers[i])) {
                std::cerr << "Failed to build initial front for tile " << i << "\n";
                return 3;
            }
            if (i < static_cast<int>(tileStates.size())) b->state = tileStates[i];
        } else if (testPattern) {
            // Camera-free: loop the pre-encoded clip through decodebin so it
            // picks the same hardware decoder rank a real stream would.
            b->source = gst_element_factory_make("multifilesrc", nullptr);
            b->decodebin = gst_element_factory_make("decodebin", nullptr);
            if (!b->source || !b->decodebin) {
                std::cerr << "Failed to create test-pattern elements for tile " << i << "\n";
                return 3;
            }
            b->tier = tiers[i];
            g_object_set(b->source, "location",
                         tierFile[static_cast<int>(tiers[i])].c_str(), nullptr);
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
            g_signal_connect(b->decodebin, "source-setup", G_CALLBACK(OnSourceSetup), &s);
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
    if (s.sweep) {
        std::cout << "sweep: re-planning the governor every " << s.sweepIntervalSec
                  << "s as focus moves across tiles; changes are applied to live "
                     "decode branches.\n";
        g_timeout_add_seconds(s.sweepIntervalSec, SweepTick, &s);
    }
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
    delete s.session;
    return 0;
}
