# P3-04 honest live diagnostics (native increment 34)

Date: 2026-08-03  
Status: built and verified on the dev box (2026-08-03)

## Decision

Build the diagnostics contract and first real observation path on the existing
governed Live workspace. Report only measurements the current media source can
actually provide; render every unsupported transport measurement as explicitly
unavailable with a reason.

## Included

- Per-governed-branch decoded-buffer observation in `GridPipeline`, plus frame
  rate measured at the compositor→Qt delivery boundary.
- Per-tile stream state: playing, connecting, stalled, paused by policy, or
  unavailable when Live video is not enabled.
- Observed decoded frame rate plus the active tier's codec and source
  resolution.
- A dedicated, tile-id-indexed `WorkspaceController` diagnostics model shared
  by grid and spatial views, kept out of the governor's hot rebuild path.
- A selected-camera diagnostics section showing stream state, codec,
  resolution, FPS, bitrate, latency, packet loss, and honest availability.
- A deterministic controller self-test and `--video --smoke-ms` runtime gate.

## Honesty rules

- Branch probes determine whether each active stream is contributing decoded
  buffers. FPS counts frames actually delivered from the compositor to Qt; it
  is not the configured nominal rate and cannot be inflated by decoders running
  ahead into leaky queues.
- Codec and resolution describe the source tier currently assigned to that
  branch.
- The present synthetic governed-grid harness exposes no network transport
  telemetry. Bitrate, latency, and packet loss therefore remain unavailable
  with source-specific reasons; zero is never used to mean unknown.
- Paused/offscreen/capacity tiles never retain a prior playing diagnostic.
- A stalled branch is distinct from a governor pause and from video-disabled.

## Explicitly excluded

- Connecting the workspace to real RTSP cameras or credentials.
- RTCP/jitter-buffer statistics, active latency probes, packet capture, or
  vendor-specific telemetry.
- Historical diagnostics, alert thresholds, event generation, persistence,
  export, P3-18 analytics, or OS/GPU telemetry.
- Playback/instant-replay diagnostics and media-pipeline redesign.

## Acceptance

1. Every tile always has a diagnostics record with typed availability fields.
2. Playing synthetic branches report observed FPS > 0 and the correct active
   codec/resolution; paused branches report policy-paused with no stale values.
3. Bitrate, latency, and packet loss are explicitly unavailable and explain
   why for the current source.
4. The selected-camera panel renders these states without adding a mutation or
   bypassing the command envelope.
5. Runtime smoke verifies observations from the real `GridPipeline`; workspace,
   component, performance, and root regression gates remain green.

## Verification evidence

- `GridPipeline` has stable decoded-buffer probes per governed branch. They
  distinguish playing, connecting, stalled, and paused without changing or
  dropping media buffers.
- FPS counts compositor frames actually delivered to `VideoItem`/Qt. An initial
  implementation counted decoder-ahead traffic and honestly exposed an invalid
  ~695 FPS result; moving the rate boundary to displayed frames corrected the
  observation to the real ~30 FPS output.
- Diagnostics use their own tile-id-indexed property and signal. The first
  nested-map implementation regressed the 64-tile Debug active p95 to 5.83 ms;
  separating the model restored it to **1.5745 ms**, below the 4.00 ms gate.
- Release 64-tile low-end spatial/video smoke loaded QML in **92 ms** and
  reported **16 active / 16 playing / 16 FPS-known / 16 media-known**, with all
  **48/48 paused tiles honest** and observed display FPS **29.7**. All 16 spatial
  consumers continued receiving their unique shared-frame crops.
- `--diagnostics-selftest` verifies default unavailable states, availability
  flags rather than fake zeroes, codec/resolution/FPS propagation, explicit
  transport reasons, no governor replan, absent observations, and stale-value
  suppression on policy pause.
- All 12 workspace gates and root regressions pass. Release spatial performance
  remains **0.0006 ms idle / 0.1691 ms active p95**. Aggregate component CTest
  is 10/11 only because the unchanged Windows ONVIF loader intermittently exits
  `0xc0000135`; focused ONVIF passes.

This verifies the diagnostics contract and the current synthetic media source.
Real-camera transport bitrate/latency/loss remain unavailable until a source
with RTSP/RTP/RTCP telemetry is connected; the UI states that explicitly.
