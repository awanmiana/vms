#include "PlaybackPipeline.h"

#ifdef VMS_WITH_GSTREAMER

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include <atomic>

#include <QImage>

#include "VideoItem.h"

struct PlaybackPipeline::Impl {
    VideoItem* sink = nullptr;
    GstElement* pipeline = nullptr;
    GstElement* filesrc = nullptr;
    GstElement* decodebin = nullptr;
    GstElement* conv = nullptr;
    GstElement* capsf = nullptr;
    GstElement* appsink = nullptr;
    std::string file;
    double rate = 1.0;
    bool playing = false;
    std::atomic<std::uint64_t> frames{0};
};

namespace {

GstFlowReturn onNewSample(GstAppSink* sink, gpointer user) {
    auto* d = static_cast<PlaybackPipeline::Impl*>(user);
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
        if (d->sink) d->sink->submitFrame(view.copy());
        d->frames.fetch_add(1);
        gst_buffer_unmap(buffer, &map);
    }
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

// decodebin exposes its decoded pad late; link it to videoconvert then.
void onPadAdded(GstElement* /*bin*/, GstPad* pad, gpointer user) {
    auto* conv = static_cast<GstElement*>(user);
    GstPad* sinkpad = gst_element_get_static_pad(conv, "sink");
    if (sinkpad && !gst_pad_is_linked(sinkpad)) gst_pad_link(pad, sinkpad);
    if (sinkpad) gst_object_unref(sinkpad);
}

}  // namespace

PlaybackPipeline::PlaybackPipeline(VideoItem* sink) : d_(new Impl) {
    d_->sink = sink;
}

PlaybackPipeline::~PlaybackPipeline() {
    stop();
    delete d_;
}

void PlaybackPipeline::stop() {
    if (d_->pipeline) {
        gst_element_set_state(d_->pipeline, GST_STATE_NULL);
        gst_object_unref(d_->pipeline);
        d_->pipeline = nullptr;
    }
    d_->filesrc = d_->decodebin = d_->conv = d_->capsf = d_->appsink = nullptr;
    d_->file.clear();
}

bool PlaybackPipeline::openFile(const std::string& path, double offsetSec,
                                std::string& error) {
    if (path.empty()) { error = "empty path"; return false; }
    if (path == d_->file && d_->pipeline) {   // same file: just seek
        seek(offsetSec);
        return true;
    }
    stop();

    d_->pipeline = gst_pipeline_new("playback");
    d_->filesrc = gst_element_factory_make("filesrc", nullptr);
    d_->decodebin = gst_element_factory_make("decodebin", nullptr);
    d_->conv = gst_element_factory_make("videoconvert", nullptr);
    d_->capsf = gst_element_factory_make("capsfilter", nullptr);
    d_->appsink = gst_element_factory_make("appsink", "pbout");
    if (!d_->pipeline || !d_->filesrc || !d_->decodebin || !d_->conv ||
        !d_->capsf || !d_->appsink) {
        error = "failed to create playback elements";
        stop();
        return false;
    }

    g_object_set(d_->filesrc, "location", path.c_str(), nullptr);
    GstCaps* rgba = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING,
                                        "RGBA", nullptr);
    g_object_set(d_->capsf, "caps", rgba, nullptr);
    gst_caps_unref(rgba);
    // Real-time paced (sync on) so playback runs at the recorded rate; drop old
    // buffers so a scrub does not back up the queue.
    g_object_set(d_->appsink, "emit-signals", TRUE, "sync", TRUE, "max-buffers",
                 2, "drop", TRUE, nullptr);
    g_signal_connect(d_->appsink, "new-sample", G_CALLBACK(onNewSample), d_);

    gst_bin_add_many(GST_BIN(d_->pipeline), d_->filesrc, d_->decodebin, d_->conv,
                     d_->capsf, d_->appsink, nullptr);
    if (!gst_element_link(d_->filesrc, d_->decodebin) ||
        !gst_element_link_many(d_->conv, d_->capsf, d_->appsink, nullptr)) {
        error = "failed to link playback pipeline";
        stop();
        return false;
    }
    g_signal_connect(d_->decodebin, "pad-added", G_CALLBACK(onPadAdded), d_->conv);

    d_->file = path;
    // Preroll so a seek lands accurately, then restore the play/pause state.
    gst_element_set_state(d_->pipeline, GST_STATE_PAUSED);
    gst_element_get_state(d_->pipeline, nullptr, nullptr, 2 * GST_SECOND);
    seek(offsetSec);
    if (d_->playing) gst_element_set_state(d_->pipeline, GST_STATE_PLAYING);
    return true;
}

void PlaybackPipeline::seek(double offsetSec) {
    if (!d_->pipeline) return;
    if (offsetSec < 0.0) offsetSec = 0.0;
    const gint64 pos = static_cast<gint64>(offsetSec * GST_SECOND);
    gst_element_seek(d_->pipeline, d_->rate, GST_FORMAT_TIME,
                     static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH |
                                               GST_SEEK_FLAG_ACCURATE),
                     GST_SEEK_TYPE_SET, pos, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
}

void PlaybackPipeline::play() {
    d_->playing = true;
    if (d_->pipeline) gst_element_set_state(d_->pipeline, GST_STATE_PLAYING);
}

void PlaybackPipeline::pause() {
    d_->playing = false;
    if (d_->pipeline) gst_element_set_state(d_->pipeline, GST_STATE_PAUSED);
}

void PlaybackPipeline::setRate(double rate) {
    if (rate <= 0.0) rate = 1.0;
    d_->rate = rate;
    if (!d_->pipeline) return;
    const double posSec = positionSec();
    const gint64 pos = static_cast<gint64>((posSec > 0 ? posSec : 0) * GST_SECOND);
    gst_element_seek(d_->pipeline, d_->rate, GST_FORMAT_TIME,
                     static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH |
                                               GST_SEEK_FLAG_ACCURATE),
                     GST_SEEK_TYPE_SET, pos, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
}

double PlaybackPipeline::positionSec() const {
    if (!d_->pipeline) return -1.0;
    gint64 pos = 0;
    if (gst_element_query_position(d_->pipeline, GST_FORMAT_TIME, &pos))
        return static_cast<double>(pos) / GST_SECOND;
    return -1.0;
}

std::uint64_t PlaybackPipeline::framesPulled() const { return d_->frames.load(); }
const std::string& PlaybackPipeline::currentFile() const { return d_->file; }
bool PlaybackPipeline::playing() const { return d_->playing; }

bool PlaybackPipeline::pumpBus(std::string& error) {
    if (!d_->pipeline) return true;
    GstBus* bus = gst_element_get_bus(d_->pipeline);
    if (!bus) return true;
    bool ok = true;
    while (GstMessage* msg = gst_bus_pop_filtered(
               bus, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS))) {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError* e = nullptr;
            gchar* dbg = nullptr;
            gst_message_parse_error(msg, &e, &dbg);
            error = e ? e->message : "playback error";
            if (e) g_error_free(e);
            g_free(dbg);
            ok = false;
        }
        // EOS: the segment ended; the caller advances to the next segment.
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
    return ok;
}

#endif // VMS_WITH_GSTREAMER
