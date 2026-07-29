// Recording pipeline — native increment 7a-media. Scope:
// ../recording-playback-P4-03-P5-02-proposal.md.
//
// The GStreamer half of optional local recording (P0-01E): a source is muxed into
// fixed-duration segment files by `splitmuxsink`, and every COMPLETED segment is
// written into the SQLite `SegmentIndex` (7a) with its wall-clock [start,end],
// path, codec, and byte size — so Playback (7c) can later resolve real recorded
// time to a real file, with honest gaps.
//
// This harness records a synthetic `--test-pattern` source, which is enough to
// verify the record->index path end-to-end on the dev box with no camera. The
// real-camera path (record a broker-opened stream, storing the ORIGINAL codec via
// depay/parse — no re-encode) reuses the same splitmuxsink->index wiring and is
// the live-blocked remainder (it shares the 4b/6c hardware/credential block).
//
// Times are wall-clock UTC 'YYYY-MM-DD HH:MM:SS' (when each fragment file was
// opened/closed) — i.e. when the footage was actually recorded, which is what
// Playback needs to map real time to a file.

#include <gst/gst.h>

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "persist/Schema.h"
#include "persist/SegmentIndex.h"
#include "persist/Store.h"

using namespace vms::persist;

namespace {

std::string nowUtc() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

// One recorded fragment as splitmuxsink hands it to us. endUtc is filled when the
// NEXT fragment opens (or at EOS for the last one).
struct Frag {
    std::string path;
    std::string startUtc;
    std::string endUtc;
};

struct RecCtx {
    std::string outDir;
    std::string cameraId;
    std::mutex mtx;
    std::vector<Frag> frags;
};

// splitmuxsink "format-location": called when each new fragment opens. We name the
// file, stamp its start, and close out the previous fragment's end. Runs on a
// streaming thread, so it only touches the mutex-guarded vector (no SQLite here).
gchar* onFormatLocation(GstElement* /*splitmux*/, guint fragmentId, gpointer user) {
    RecCtx* c = static_cast<RecCtx*>(user);
    const std::string now = nowUtc();
    std::string path =
        c->outDir + "/" + c->cameraId + "_" + std::to_string(fragmentId) + ".mp4";
    {
        std::lock_guard<std::mutex> lock(c->mtx);
        if (!c->frags.empty() && c->frags.back().endUtc.empty())
            c->frags.back().endUtc = now;
        c->frags.push_back({path, now, ""});
    }
    return g_strdup(path.c_str());
}

}  // namespace

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    bool testPattern = false;
    std::string codec = "h264";
    std::string db = "rec.db";
    std::string outDir = "rec";
    std::string cameraId = "cam-1";
    int segmentSeconds = 3;
    int seconds = 15;
    int w = 640, h = 480;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--test-pattern")                    testPattern = true;
        else if (a == "--codec" && i + 1 < argc)      codec = argv[++i];
        else if (a == "--db" && i + 1 < argc)         db = argv[++i];
        else if (a == "--out-dir" && i + 1 < argc)    outDir = argv[++i];
        else if (a == "--camera-id" && i + 1 < argc)  cameraId = argv[++i];
        else if (a == "--segment-seconds" && i + 1 < argc) segmentSeconds = std::atoi(argv[++i]);
        else if (a == "--seconds" && i + 1 < argc)    seconds = std::atoi(argv[++i]);
        else if (a == "--width" && i + 1 < argc)      w = std::atoi(argv[++i]);
        else if (a == "--height" && i + 1 < argc)     h = std::atoi(argv[++i]);
    }

    if (!testPattern) {
        std::cerr << "usage: vms_record --test-pattern [--codec h264|h265] "
                     "[--db rec.db] [--out-dir rec] [--camera-id cam-1] "
                     "[--segment-seconds 3] [--seconds 15]\n"
                     "  (recording a broker-opened real camera stream is the "
                     "live-blocked remainder; this harness records a synthetic "
                     "source to verify the record->index path)\n";
        return 2;
    }
    if (codec != "h264" && codec != "h265") {
        std::cerr << "--codec must be h264 or h265\n";
        return 2;
    }

    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    // Store an ABSOLUTE segment directory so the index is location-independent:
    // Playback opens the files by the path recorded here, from any working dir.
    {
        std::error_code ae;
        const std::filesystem::path abs = std::filesystem::absolute(outDir, ae);
        if (!ae) outDir = abs.string();
    }

    const std::string enc = (codec == "h265")
        ? "x265enc key-int-max=25"
        : "x264enc key-int-max=25 tune=zerolatency";
    const std::string parse = (codec == "h265") ? "h265parse" : "h264parse";
    const std::string desc =
        "videotestsrc is-live=true pattern=18 ! video/x-raw,width=" +
        std::to_string(w) + ",height=" + std::to_string(h) +
        ",framerate=25/1 ! " + enc + " ! " + parse + " ! splitmuxsink name=sink";

    GError* err = nullptr;
    GstElement* pipeline = gst_parse_launch(desc.c_str(), &err);
    if (!pipeline) {
        std::cerr << "pipeline build failed: " << (err ? err->message : "?") << "\n";
        if (err) g_error_free(err);
        return 1;
    }

    RecCtx ctx;
    ctx.outDir = outDir;
    ctx.cameraId = cameraId;

    GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    g_object_set(sink, "max-size-time",
                 static_cast<guint64>(segmentSeconds) * GST_SECOND, nullptr);
    g_signal_connect(sink, "format-location", G_CALLBACK(onFormatLocation), &ctx);
    gst_object_unref(sink);

    std::cout << "vms_record: " << codec << " " << w << "x" << h
              << " -> " << outDir << "/ (" << segmentSeconds << "s segments, "
              << seconds << "s run), indexing into " << db
              << " as camera '" << cameraId << "'\n";

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    GstBus* bus = gst_element_get_bus(pipeline);
    const gint64 startUs = g_get_monotonic_time();
    bool eosSent = false, hadError = false;
    for (;;) {
        GstMessage* msg = gst_bus_timed_pop_filtered(
            bus, 200 * GST_MSECOND,
            static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
        if (msg) {
            if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
                GError* e = nullptr;
                gchar* dbg = nullptr;
                gst_message_parse_error(msg, &e, &dbg);
                std::cerr << "pipeline error: " << (e ? e->message : "?") << "\n";
                if (e) g_error_free(e);
                g_free(dbg);
                hadError = true;
            }
            gst_message_unref(msg);
            break;  // ERROR or EOS
        }
        if (!eosSent && (g_get_monotonic_time() - startUs) >=
                            static_cast<gint64>(seconds) * 1000000) {
            gst_element_send_event(pipeline, gst_event_new_eos());
            eosSent = true;
        }
    }
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    // Close out the final fragment (no next fragment opened to stamp its end).
    {
        std::lock_guard<std::mutex> lock(ctx.mtx);
        if (!ctx.frags.empty() && ctx.frags.back().endUtc.empty())
            ctx.frags.back().endUtc = nowUtc();
    }

    if (hadError) return 1;

    // Index every completed fragment. This is the only place SQLite is touched,
    // and it runs on the main thread after the pipeline has stopped.
    Store store;
    if (Error e = store.open(db); !e) {
        std::cerr << "db open failed: " << e.message << "\n";
        return 1;
    }
    if (Error e = store.migrate(coreMigrations()); !e) {
        std::cerr << "migrate failed: " << e.message << "\n";
        return 1;
    }
    SegmentIndex idx(store);

    int indexed = 0;
    std::int64_t totalBytes = 0;
    for (const Frag& f : ctx.frags) {
        if (f.endUtc.empty() || f.endUtc <= f.startUtc) continue;  // too short to be honest
        std::error_code fe;
        const std::uintmax_t sz = std::filesystem::file_size(f.path, fe);
        if (fe) continue;  // file missing/unreadable -> do not index footage we cannot back
        Segment seg;
        seg.cameraId = cameraId;
        seg.startUtc = f.startUtc;
        seg.endUtc = f.endUtc;
        seg.path = f.path;
        seg.codec = codec;
        seg.bytes = static_cast<std::int64_t>(sz);
        std::int64_t id = 0;
        if (Error e = idx.add(seg, id); !e) {
            std::cerr << "index add failed for " << f.path << ": " << e.message << "\n";
            continue;
        }
        ++indexed;
        totalBytes += seg.bytes;
        std::cout << "  indexed [" << f.startUtc << " .. " << f.endUtc << "] "
                  << f.path << " (" << seg.bytes << " bytes)\n";
    }

    std::cout << "recorded+indexed " << indexed << " segment(s), "
              << totalBytes << " bytes total\n";

    if (indexed > 0) {
        std::vector<AvailabilitySpan> av;
        idx.availability(cameraId, ctx.frags.front().startUtc,
                         ctx.frags.back().endUtc, av);
        std::cout << "availability over the recorded window: " << av.size() << " span(s):\n";
        for (const AvailabilitySpan& s : av)
            std::cout << "  [" << s.startUtc << " .. " << s.endUtc << "] "
                      << SpanStateName(s.state) << " (" << s.sources << " source)\n";
    }

    if (indexed == 0) {
        std::cerr << "FAILED: no segments recorded/indexed\n";
        return 1;
    }
    std::cout << "PASS: recorded segments written to files and indexed\n";
    return 0;
}
