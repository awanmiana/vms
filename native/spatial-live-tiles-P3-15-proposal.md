# P3-15 adaptive spatial live tiles (native increment 33)

Date: 2026-08-01  
Status: approved by the operator's directive to identify and start the next build

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
