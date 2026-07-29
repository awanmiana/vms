#pragma once

// P5-02 / inc 7c-3 — decode a recorded segment file into the Playback pane.
//
// The Playback counterpart of GridPipeline: instead of a live governed grid, it
// decodes ONE recorded file (filesrc -> decodebin -> videoconvert -> RGBA ->
// appsink) and pushes finished frames into a VideoItem, exactly the interop-free
// appsink->QSGTexture route the live grid uses. Real-time paced (sink sync on)
// and rate-controllable, so the operator's scrub/play/speed on the timeline
// (PlaybackController) drives actual moving video. GStreamer stays behind this
// class (pImpl); the header pulls in no gst headers.

#include <cstdint>
#include <string>

class VideoItem;

class PlaybackPipeline {
public:
    explicit PlaybackPipeline(VideoItem* sink);
    ~PlaybackPipeline();

    PlaybackPipeline(const PlaybackPipeline&) = delete;
    PlaybackPipeline& operator=(const PlaybackPipeline&) = delete;

    // Play (or switch to) a recorded file, seeking to offsetSec. Rebuilds the
    // pipeline when the file changes; a same-file call just seeks. Preserves the
    // current play/pause state and rate. Returns false + sets error on failure.
    bool openFile(const std::string& path, double offsetSec, std::string& error);

    void play();
    void pause();
    void setRate(double rate);           // 1.0 / 2.0 / 4.0
    void seek(double offsetSec);         // within the current file
    void stop();

    // Current decode position within the open file, in seconds (-1 if unknown).
    double positionSec() const;
    // Total frames pushed to the sink (verification hook for the offscreen smoke).
    std::uint64_t framesPulled() const;
    const std::string& currentFile() const;
    bool playing() const;

    // Drain the GStreamer bus for errors/EOS; call from a Qt timer on the GUI
    // thread. Returns false and sets error on a fatal pipeline error.
    bool pumpBus(std::string& error);

    struct Impl;

private:
    Impl* d_;
};
