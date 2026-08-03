#pragma once

// P3-14 slice 2b — the governed d3d11-composited grid as the video source that
// renders *under* the honest per-tile state chrome.
//
// This is the same decode+composite pipeline vms_grid proved (per-tile hardware
// decode -> d3d11compositor into one surface), with the display sink swapped for
// `d3d11download -> appsink` so the finished frames land in a VideoItem texture
// instead of a separate native window. Each tile is placed at its grid cell so
// the QML chrome (which divides the same rectangle into the same rows x cols)
// lines up cell-for-cell. Paused tiles are composited black — never a stale
// frame — matching the governor's honest state.
//
// The pipeline is built from a fixed initial plan (slice 2b-1); re-planning the
// live video on a focus sweep is 2b-2. GStreamer stays entirely behind this
// class (pImpl), so the header pulls in no gst headers.

#include <string>
#include <vector>

#include "governor/Governor.h"

class VideoItem;

struct GridTileDiagnostics {
    int tileId = -1;
    bool receiving = false;
    double fps = 0.0;
    std::string streamState;  // playing / connecting / stalled / paused
    std::string codec;
    int width = 0;
    int height = 0;
};

class GridPipeline {
public:
    // `tiers` is indexed by tile id (0..cols*rows-1); its size is the tile count.
    GridPipeline(VideoItem* sink, int cols, int rows,
                 std::vector<vms::Tier> tiers, std::string codec);
    ~GridPipeline();

    GridPipeline(const GridPipeline&) = delete;
    GridPipeline& operator=(const GridPipeline&) = delete;

    // Encode the per-tier clips, assemble the grid, go PLAYING. Returns false and
    // sets `error` on any failure (nothing is left running). When `sweepable` is
    // true, every tier's clip is encoded up front (not just those in the initial
    // plan) so a later applyPlan() can move any tile to any tier.
    bool start(bool sweepable, std::string& error);
    void stop();

    // Re-plan the LIVE grid to `tiers` (indexed by tile id): take the pipeline to
    // NULL, rebuild each branch's decode front at its new tier, and go PLAYING —
    // the reliable path vms_grid uses (a hot-swap into a running pipeline does not
    // survive the D3D12 decoder + reused compositor pad). A no-op if nothing
    // changed. The stable queue -> upload -> compositor-pad spine never moves.
    bool applyPlan(const std::vector<vms::Tier>& tiers, std::string& error);

    // Drain the GStreamer bus for errors/EOS; call from a Qt timer on the GUI
    // thread. Returns false if a fatal pipeline error was seen.
    bool pumpBus(std::string& error);

    // P3-04 / inc 34: a snapshot of facts observed at each governed decode
    // branch. Branch probes establish playing/connecting/stalled; FPS counts
    // composited frames actually delivered to Qt (raw decoders can run far
    // ahead into leaky queues and would over-report). Transport metrics are
    // deliberately not claimed because the current synthetic source exposes none.
    std::vector<GridTileDiagnostics> diagnostics();

    std::string summary() const;   // encoder + per-tier decoder(s), for the console

    // Defined in the .cpp; declared public only so the file-local pipeline
    // helpers there can name it. Not part of the API — do not use.
    struct Impl;

private:
    Impl* d_;
};
