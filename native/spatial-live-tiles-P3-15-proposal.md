# P3-15 adaptive spatial live tiles (native increment 33)

Date: 2026-08-01  
Status: built and verified on the dev box (2026-08-03)

## Decision

Use a progressive representation instead of choosing one representation for
every zoom level:

- **Site:** compact state pins; no video surface.
- **Wing:** honest state cards; no video surface.
- **Room:** live imagery inside tiles that the governor says are decoding;
  paused/capacity/offscreen tiles remain honest state cards.

This keeps the overview legible and cheap while making close spatial navigation
operationally useful.

## Included

- Extend the existing `VideoItem` bridge so one decoded/composited frame source
  can feed multiple crop consumers without opening another camera session.
- Crop each spatial consumer to its tile cell in the existing governed grid
  composite; preserve the current controller's tile-id/row/column mapping.
- Instantiate/show spatial video only at room zoom and only for `live` or
  `degraded` tile states. Existing site pins and wing cards remain.
- Keep state, tier, focus, priority, and FOV chrome above the imagery.
- Frame counters and offscreen smoke evidence proving spatial consumers receive
  frames from the one source.
- Re-run the increment-32 performance gate so the non-video controller budget
  remains unchanged.

## Constraints and honesty

- The existing `GridPipeline` remains the single decoder/compositor owner. A
  spatial tile subscribes to its already-produced frame; it does not create a
  duplicate RTSP/GStreamer session or decode branch.
- Video appears only under `--video`. Without it, spatial tiles keep the honest
  state-only presentation.
- A paused tile never shows a stale crop as live. Zooming out removes video
  presentation but does not invent a snapshot.
- The source is still the current synthetic governed-grid harness until the
  separately tracked broker-backed real-camera workspace path is completed.

## Explicitly excluded

- Per-tile independent pipelines, duplicate camera connections, new recording
  sources, compositor hot-swap redesign, WebRTC/HLS, or real-camera credentials.
- Snapshot caching, historical imagery, GIS/map providers, PTZ-driven FOV,
  user-selectable media modes, or changes to governor admission.
- P3-18 analytics/operations and premises administration.

## Acceptance

1. At room zoom, every live/degraded spatial tile samples its correct composite
   cell; site zoom remains pins and wing zoom remains state cards.
2. All spatial consumers receive frames from one `GridPipeline` source and no
   additional media pipeline/session is created.
3. Paused/offscreen/capacity tiles never display a live/stale crop.
4. `--video --spatial` offscreen smoke receives frames in spatial consumers and
   exits cleanly with no QML errors.
5. The spatial performance gate, command coverage, workspace gates, component
   tests, and root regressions remain green.

## Verification evidence

- `VideoItem` now supports a shared `frameSource` plus row-major crop metadata.
  Consumers share the source `QImage` until their render-thread crop; the
  existing `GridPipeline` remains the only session/decode/compositor owner.
- The spatial delegate instantiates a consumer only at room zoom and only for
  honest `live`/`degraded` states. Site remains pin-only; wing and every paused
  state remain state-only. Existing focus, tier, priority, state, and FOV chrome
  stays above the imagery.
- `--spatial-selftest` pins crop geometry for uneven 1281x801 composites,
  including the rounded final cell and the safe invalid-index fallback.
- Release offscreen smoke with `--no-persist --count 64 --profile lowend
  --video --spatial --smoke-ms 3000` loaded QML in **85 ms** and reported
  **64 delegates / 16 eligible / 16 consumers / 16 receiving / 223 delivered
  frames / 16 unique crop ids**. The other 48 tiles had no consumer.
- The repeated Release performance gate passed at **0.0006 ms idle p95** and
  **0.1874 ms forced-replan p95** over 5,000 samples. Debug passed at
  **0.0040 / 1.7165 ms p95** over 2,000 samples.
- Native build, all 11 workspace gates, and the complete root regression suite
  pass. Aggregate component CTest is 10/11 only because the unchanged Windows
  ONVIF loader intermittently exits `0xc0000135`; the focused ONVIF CTest passes.

The verification source is the existing synthetic governed-grid media harness,
not a claim of live-camera credential/device validation. That remains the
separately tracked broker/media hardware gate.
