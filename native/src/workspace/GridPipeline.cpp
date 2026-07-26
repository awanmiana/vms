#include "GridPipeline.h"

#ifdef VMS_WITH_GSTREAMER

#include "VideoItem.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include <QImage>

#include <iostream>
#include <set>
#include <string>

namespace {

// First registered factory from a preference-ordered list, or nullptr.
const char* PickFactory(const char* const* names) {
    for (const char* const* p = names; *p; ++p) {
        if (GstElementFactory* f = gst_element_factory_find(*p)) {
            gst_object_unref(f);
            return *p;
        }
    }
    return nullptr;
}

// The active hardware/software decoder inside a decodebin (for reporting).
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
                    const gchar* name =
                        gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(f));
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
    return found;
}

// Encode one synthetic clip (ported from vms_grid::EncodePattern). Camera-typical
// I420 4:2:0 so decodebin selects the same hardware decoder a real stream would.
bool EncodePattern(const std::string& codec, int sw, int sh, int frames,
                   const std::string& outPath, std::string& encoderUsed) {
    const char* h265[] = {"x265enc", "qsvh265enc", "nvh265enc", nullptr};
    const char* h264[] = {"x264enc", "qsvh264enc", "nvh264enc", nullptr};
    const bool isH264 = (codec == "h264");
    const char* parseName = isH264 ? "h264parse" : "h265parse";
    const char* encName = PickFactory(isH264 ? h264 : h265);
    if (!encName) return false;
    encoderUsed = encName;

    GstElement* pipe = gst_pipeline_new("encode");
    GstElement* src = gst_element_factory_make("videotestsrc", nullptr);
    GstElement* conv = gst_element_factory_make("videoconvert", nullptr);
    GstElement* caps = gst_element_factory_make("capsfilter", nullptr);
    GstElement* enc = gst_element_factory_make(encName, nullptr);
    GstElement* parse = gst_element_factory_make(parseName, nullptr);
    GstElement* sink = gst_element_factory_make("filesink", nullptr);
    if (!pipe || !src || !conv || !caps || !enc || !parse || !sink) {
        if (pipe) gst_object_unref(pipe);
        return false;
    }

    g_object_set(src, "num-buffers", frames, "pattern", 0 /*smpte*/,
                 "is-live", FALSE, nullptr);
    GstCaps* c = gst_caps_new_simple(
        "video/x-raw", "width", G_TYPE_INT, sw, "height", G_TYPE_INT, sh,
        "framerate", GST_TYPE_FRACTION, 25, 1,
        "format", G_TYPE_STRING, "I420", nullptr);
    g_object_set(caps, "caps", c, nullptr);
    gst_caps_unref(c);
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(enc), "key-int-max"))
        g_object_set(enc, "key-int-max", 25, nullptr);
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(parse), "config-interval"))
        g_object_set(parse, "config-interval", -1, nullptr);
    g_object_set(sink, "location", outPath.c_str(), nullptr);

    gst_bin_add_many(GST_BIN(pipe), src, conv, caps, enc, parse, sink, nullptr);
    GstCaps* bs = gst_caps_new_simple(
        isH264 ? "video/x-h264" : "video/x-h265",
        "stream-format", G_TYPE_STRING, "byte-stream",
        "alignment", G_TYPE_STRING, "au", nullptr);
    const gboolean tail = gst_element_link_filtered(parse, sink, bs);
    gst_caps_unref(bs);
    bool ok = false;
    if (gst_element_link_many(src, conv, caps, enc, parse, nullptr) && tail) {
        gst_element_set_state(pipe, GST_STATE_PLAYING);
        GstBus* bus = gst_element_get_bus(pipe);
        GstMessage* msg = gst_bus_timed_pop_filtered(
            bus, GST_CLOCK_TIME_NONE,
            static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
        if (msg) {
            ok = GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS;
            gst_message_unref(msg);
        }
        gst_object_unref(bus);
    }
    gst_element_set_state(pipe, GST_STATE_NULL);
    gst_object_unref(pipe);
    return ok;
}

struct Branch {
    int index = 0;
    vms::Tier tier = vms::Tier::Paused;
    GstElement* source = nullptr;     // multifilesrc (decode) or videotestsrc (paused)
    GstElement* decodebin = nullptr;  // null for paused/black tiles
    GstElement* queue = nullptr;
    GstElement* upload = nullptr;
    GstPad* compPad = nullptr;
    bool linked = false;
    std::string decoder;
};

// Link the decoded video pad to the branch queue (ported from vms_grid).
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
    if (gst_pad_link(pad, qsink) == GST_PAD_LINK_OK) {
        b->decoder = FindDecoderName(GST_BIN(dbin));
        b->linked = true;
    }
    gst_object_unref(qsink);
}

} // namespace

struct GridPipeline::Impl {
    VideoItem* sink = nullptr;
    int cols = 1, rows = 1;
    std::vector<vms::Tier> tiers;
    std::string codec;

    GstElement* pipeline = nullptr;
    GstElement* comp = nullptr;
    GstElement* appsink = nullptr;
    GstBus* bus = nullptr;
    std::vector<Branch*> branches;
    std::string tierFile[4];   // indexed by (int)Tier: 1=thumb 2=sub 3=main
    std::string encoderUsed;

    // The composited surface is a fixed 1080p; each cell is an equal integer
    // fraction so it aligns with the QML chrome grid.
    int outW = 1920, outH = 1080;
};

namespace {

GstFlowReturn onNewSample(GstAppSink* sink, gpointer user) {
    auto* item = static_cast<VideoItem*>(user);
    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (!sample) return GST_FLOW_OK;
    GstCaps* caps = gst_sample_get_caps(sample);
    GstStructure* s = caps ? gst_caps_get_structure(caps, 0) : nullptr;
    int w = 0, h = 0;
    if (s) {
        gst_structure_get_int(s, "width", &w);
        gst_structure_get_int(s, "height", &h);
    }
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (w > 0 && h > 0 && buffer && gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        const QImage view(map.data, w, h, w * 4, QImage::Format_RGBA8888);
        item->submitFrame(view.copy());
        gst_buffer_unmap(buffer, &map);
    }
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

void resForTier(vms::Tier t, int& w, int& h) {
    switch (t) {
        case vms::Tier::Sub:   w = 640;  h = 480;  break;
        case vms::Tier::Thumb: w = 320;  h = 240;  break;
        default:               w = 1920; h = 1080; break;  // Main
    }
}

} // namespace

// Build the decode front for a tier and link it to the branch's queue. Paused
// tiles get a paced black source and no decoder (never a stale frame); decode
// tiers loop the pre-encoded clip through decodebin. The queue/upload/compositor
// spine is assumed already in the pipeline; the caller drives the pipeline state.
static bool buildFront(GridPipeline::Impl* d, Branch* b, vms::Tier tier,
                       std::string& err) {
    b->tier = tier;
    b->linked = false;

    if (tier == vms::Tier::Paused) {
        b->source = gst_element_factory_make("videotestsrc", nullptr);
        if (!b->source) { err = "videotestsrc create failed"; return false; }
        g_object_set(b->source, "pattern", 2 /*black*/, "is-live", TRUE, nullptr);
        gst_bin_add(GST_BIN(d->pipeline), b->source);
        if (!gst_element_link(b->source, b->queue)) {
            err = "failed to link black source -> queue";
            return false;
        }
        b->decoder = "(paused)";
        b->linked = true;
        return true;
    }

    b->source = gst_element_factory_make("multifilesrc", nullptr);
    b->decodebin = gst_element_factory_make("decodebin", nullptr);
    if (!b->source || !b->decodebin) {
        err = "failed to create decode elements";
        return false;
    }
    g_object_set(b->source, "location",
                 d->tierFile[static_cast<int>(tier)].c_str(), nullptr);
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(b->source), "loop"))
        g_object_set(b->source, "loop", TRUE, nullptr);
    g_signal_connect(b->decodebin, "pad-added", G_CALLBACK(OnPadAdded), b);
    gst_bin_add_many(GST_BIN(d->pipeline), b->source, b->decodebin, nullptr);
    if (!gst_element_link(b->source, b->decodebin)) {
        err = "failed to link source -> decodebin";
        return false;
    }
    b->decoder = "(pending)";
    return true;
}

// Detach and destroy whatever currently feeds this branch's queue (ported from
// vms_grid). The stable queue -> upload -> compositor pad is left untouched.
static void teardownFront(GridPipeline::Impl* d, Branch* b) {
    GstPad* qsink = gst_element_get_static_pad(b->queue, "sink");
    if (GstPad* peer = gst_pad_get_peer(qsink)) {
        gst_pad_unlink(peer, qsink);
        gst_object_unref(peer);
    }
    gst_object_unref(qsink);
    if (b->decodebin) {
        gst_element_set_state(b->decodebin, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(d->pipeline), b->decodebin);
        b->decodebin = nullptr;
    }
    if (b->source) {
        gst_element_set_state(b->source, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(d->pipeline), b->source);
        b->source = nullptr;
    }
    b->linked = false;
}

GridPipeline::GridPipeline(VideoItem* sink, int cols, int rows,
                           std::vector<vms::Tier> tiers, std::string codec)
    : d_(new Impl) {
    d_->sink = sink;
    d_->cols = cols > 0 ? cols : 1;
    d_->rows = rows > 0 ? rows : 1;
    d_->tiers = std::move(tiers);
    d_->codec = std::move(codec);
}

GridPipeline::~GridPipeline() {
    stop();
    delete d_;
}

bool GridPipeline::start(bool sweepable, std::string& error) {
    // 1) Encode the clips. A sweepable grid can move any tile to any tier at run
    // time, so every tier's clip must exist up front; otherwise just those the
    // initial plan uses.
    std::set<vms::Tier> need;
    if (sweepable) {
        need = {vms::Tier::Main, vms::Tier::Sub, vms::Tier::Thumb};
    } else {
        for (vms::Tier t : d_->tiers)
            if (t != vms::Tier::Paused) need.insert(t);
    }
    if (need.empty()) {
        error = "governor paused every tile; no video to decode";
        return false;
    }
    for (vms::Tier t : need) {
        int w = 0, h = 0;
        resForTier(t, w, h);
        const std::string f = std::string(g_get_tmp_dir()) +
                              "/vms_workspace_" + std::to_string(w) + "x" +
                              std::to_string(h) + "." + d_->codec;
        std::string enc;
        if (!EncodePattern(d_->codec, w, h, 250, f, enc)) {
            error = "failed to encode the " + std::to_string(w) + "x" +
                    std::to_string(h) + " test clip";
            return false;
        }
        d_->tierFile[static_cast<int>(t)] = f;
        d_->encoderUsed = enc;
    }

    // 2) Core: d3d11compositor -> d3d11download -> RGBA -> appsink.
    d_->pipeline = gst_pipeline_new("workspace-grid");
    d_->comp = gst_element_factory_make("d3d11compositor", "mix");
    GstElement* download = gst_element_factory_make("d3d11download", nullptr);
    GstElement* conv = gst_element_factory_make("videoconvert", nullptr);
    GstElement* scale = gst_element_factory_make("videoscale", nullptr);
    GstElement* capsf = gst_element_factory_make("capsfilter", nullptr);
    d_->appsink = gst_element_factory_make("appsink", "out");
    if (!d_->pipeline || !d_->comp || !download || !conv || !scale || !capsf ||
        !d_->appsink) {
        error = "failed to create core elements (need the GStreamer d3d11 plugin"
                " with d3d11compositor + d3d11download, and appsink)";
        return false;
    }

    GstCaps* outCaps = gst_caps_new_simple(
        "video/x-raw", "format", G_TYPE_STRING, "RGBA",
        "width", G_TYPE_INT, d_->outW, "height", G_TYPE_INT, d_->outH, nullptr);
    g_object_set(capsf, "caps", outCaps, nullptr);
    gst_caps_unref(outCaps);

    g_object_set(d_->appsink, "emit-signals", TRUE, "max-buffers", 1, "drop",
                 TRUE, "sync", TRUE, nullptr);
    g_signal_connect(d_->appsink, "new-sample", G_CALLBACK(onNewSample),
                     d_->sink);

    gst_bin_add_many(GST_BIN(d_->pipeline), d_->comp, download, conv, scale,
                     capsf, d_->appsink, nullptr);
    if (!gst_element_link_many(d_->comp, download, conv, scale, capsf,
                               d_->appsink, nullptr)) {
        error = "failed to link compositor -> download -> appsink";
        return false;
    }

    // 3) One branch per tile, placed at its grid cell.
    const int cellW = d_->outW / d_->cols;
    const int cellH = d_->outH / d_->rows;
    const int n = static_cast<int>(d_->tiers.size());
    for (int i = 0; i < n; ++i) {
        auto* b = new Branch();
        b->index = i;
        b->tier = d_->tiers[i];
        b->queue = gst_element_factory_make("queue", nullptr);
        b->upload = gst_element_factory_make("d3d11upload", nullptr);
        if (!b->queue || !b->upload) {
            error = "failed to create branch elements";
            delete b;
            return false;
        }
        g_object_set(b->queue, "leaky", 2, "max-size-buffers", 3,
                     "max-size-time", static_cast<guint64>(0), "max-size-bytes",
                     0, nullptr);
        gst_bin_add_many(GST_BIN(d_->pipeline), b->queue, b->upload, nullptr);

        std::string ferr;
        if (!buildFront(d_, b, d_->tiers[i], ferr)) {
            error = ferr;
            delete b;
            return false;
        }

        if (!gst_element_link(b->queue, b->upload)) {
            error = "failed to link queue -> upload";
            delete b;
            return false;
        }

        b->compPad = gst_element_request_pad_simple(d_->comp, "sink_%u");
        const int cx = (i % d_->cols) * cellW;
        const int cy = (i / d_->cols) * cellH;
        g_object_set(b->compPad, "xpos", cx, "ypos", cy, "width", cellW,
                     "height", cellH, nullptr);
        GstPad* upSrc = gst_element_get_static_pad(b->upload, "src");
        const bool linkedPad =
            gst_pad_link(upSrc, b->compPad) == GST_PAD_LINK_OK;
        gst_object_unref(upSrc);
        if (!linkedPad) {
            error = "failed to link upload -> compositor pad";
            delete b;
            return false;
        }
        d_->branches.push_back(b);
    }

    d_->bus = gst_element_get_bus(d_->pipeline);
    if (gst_element_set_state(d_->pipeline, GST_STATE_PLAYING) ==
        GST_STATE_CHANGE_FAILURE) {
        error = "pipeline failed to reach PLAYING";
        return false;
    }
    return true;
}

bool GridPipeline::applyPlan(const std::vector<vms::Tier>& tiers,
                            std::string& error) {
    if (!d_->pipeline) {
        error = "pipeline not started";
        return false;
    }

    auto targetFor = [&](int id) {
        return (id >= 0 && id < static_cast<int>(tiers.size()))
                   ? tiers[id]
                   : vms::Tier::Paused;
    };

    bool changed = false;
    for (Branch* b : d_->branches)
        if (targetFor(b->index) != b->tier) { changed = true; break; }
    if (!changed) return true;   // steady load: no rebuild, no flicker

    // Whole-graph NULL -> rebuild every front -> PLAYING (the reliable path; a
    // hot-swap into the running pipeline wedges the D3D12 decoder).
    gst_element_set_state(d_->pipeline, GST_STATE_NULL);
    gst_element_get_state(d_->pipeline, nullptr, nullptr, GST_CLOCK_TIME_NONE);

    bool ok = true;
    for (Branch* b : d_->branches) {
        teardownFront(d_, b);
        std::string ferr;
        if (!buildFront(d_, b, targetFor(b->index), ferr)) {
            error = "tile " + std::to_string(b->index) + ": " + ferr;
            ok = false;
        }
    }

    if (gst_element_set_state(d_->pipeline, GST_STATE_PLAYING) ==
        GST_STATE_CHANGE_FAILURE) {
        error = "pipeline failed to return to PLAYING after re-plan";
        return false;
    }
    return ok;
}

bool GridPipeline::pumpBus(std::string& error) {
    if (!d_->bus) return true;
    GstMessage* msg = gst_bus_pop_filtered(
        d_->bus, static_cast<GstMessageType>(GST_MESSAGE_ERROR));
    if (!msg) return true;
    GError* e = nullptr;
    gchar* dbg = nullptr;
    gst_message_parse_error(msg, &e, &dbg);
    error = e ? e->message : "unknown pipeline error";
    if (e) g_error_free(e);
    g_free(dbg);
    gst_message_unref(msg);
    return false;
}

void GridPipeline::stop() {
    if (d_->bus) {
        gst_object_unref(d_->bus);
        d_->bus = nullptr;
    }
    if (d_->pipeline) {
        gst_element_set_state(d_->pipeline, GST_STATE_NULL);
        gst_object_unref(d_->pipeline);
        d_->pipeline = nullptr;
    }
    for (Branch* b : d_->branches) delete b;
    d_->branches.clear();
}

std::string GridPipeline::summary() const {
    std::set<std::string> decoders;
    for (Branch* b : d_->branches)
        if (!b->decoder.empty() && b->decoder != "(paused)")
            decoders.insert(b->decoder);
    std::string out = "encoded with " + d_->encoderUsed + "; decoders:";
    if (decoders.empty()) {
        out += " (pending)";
    } else {
        for (const auto& d : decoders) out += " " + d;
    }
    return out;
}

#else // !VMS_WITH_GSTREAMER — stubs so the pure-Qt build still links.

GridPipeline::GridPipeline(VideoItem*, int, int, std::vector<vms::Tier>,
                           std::string)
    : d_(nullptr) {}
GridPipeline::~GridPipeline() {}
bool GridPipeline::start(bool, std::string& error) {
    error = "built without GStreamer";
    return false;
}
void GridPipeline::stop() {}
bool GridPipeline::applyPlan(const std::vector<vms::Tier>&, std::string&) {
    return true;
}
bool GridPipeline::pumpBus(std::string&) { return true; }
std::string GridPipeline::summary() const { return {}; }

#endif // VMS_WITH_GSTREAMER
