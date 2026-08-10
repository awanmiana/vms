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

#include <cstdint>
#include <string>
#include <vector>

#include "governor/Governor.h"

class VideoItem;
namespace vms::media {
class GridLiveSource;
struct RtspTransportPolicy;
}

struct GridTileDiagnostics {
    int tileId = -1;
    bool receiving = false;
    double fps = 0.0;
    std::string streamState;  // playing / connecting / stalled / paused
    std::string reason;       // credential-safe state/unavailability explanation
    std::string codec;
    int width = 0;
    int height = 0;
};

struct GridReplanStats {
    std::uint64_t appliedPlans = 0;
    std::uint64_t branchFrontRebuilds = 0;
    std::uint64_t fullPipelineFallbacks = 0;
    int lastChangedBranches = 0;
    double lastApplyMilliseconds = 0.0;
    bool lastApplyUsedFallback = false;
};

class GridPipeline {
public:
    // `tiers` is indexed by tile id (0..cols*rows-1); its size is the tile count.
    GridPipeline(VideoItem* sink, int cols, int rows,
                 std::vector<vms::Tier> tiers, std::string codec);
    // Real inventory-backed mode. `liveSource` is non-owning and must outlive
    // the pipeline. Each active branch leases only its governed tier's stream.
    GridPipeline(VideoItem* sink, int cols, int rows,
                 std::vector<vms::Tier> tiers,
                 vms::media::GridLiveSource* liveSource,
                 const vms::media::RtspTransportPolicy& rtspPolicy);
    ~GridPipeline();

    GridPipeline(const GridPipeline&) = delete;
    GridPipeline& operator=(const GridPipeline&) = delete;

    // Synthetic mode encodes per-tier clips; live mode obtains broker-backed
    // RTSP leases. Both assemble the grid and go PLAYING. A live branch that
    // cannot acquire its source is isolated as unavailable/black.
    bool start(bool sweepable, std::string& error);
    void stop();

    // Re-plan the LIVE grid to `tiers` (indexed by tile id). The graph briefly
    // pauses so VideoItem retains the last complete frame, then only changed
    // branches are replaced with new decode/queue/upload/compositor-pad chains.
    // Unchanged decoders and compositor pads remain alive. A full NULL rebuild
    // is a counted reliability fallback, never a silent path. Same-layout only;
    // layout geometry changes still rebuild the complete pipeline.
    bool applyPlan(const std::vector<vms::Tier>& tiers, std::string& error);
    GridReplanStats replanStats() const;

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
