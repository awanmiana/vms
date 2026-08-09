# P3-14 continuity-preserving live re-plan (native increment 41)

Date: 2026-08-09  
Status: approved, built, and verified under the operator's directive to
identify what is next and continue building

## Decision

Replace the same-layout governor apply's whole-pipeline NULL rebuild with a
continuity-preserving changed-branch replacement. The pipeline briefly enters
PAUSED so the last complete `VideoItem` frame remains visible. Each changed
tile receives a new decode, queue, upload, and compositor-pad chain; unchanged
decode branches and compositor pads remain alive. The graph then resumes
PLAYING.

This avoids the failed design of mutating a running D3D12 decoder front while
reusing its compositor pad. It is a bounded state-boundary replacement, not a
claim of zero-latency crossfade or transport-session continuity.

## Included

- Same-layout focus, priority, desired-tier, viewport, and capacity re-plans.
- Detection of exactly which tile tiers changed; no-op plans do no media work.
- A short PAUSED boundary that retains the last complete Qt frame.
- Complete replacement branches with fresh compositor request pads for changed
  tiles, preventing reuse of the pad/decoder combination that previously
  wedged repeated H.265 changes.
- Preservation of all unchanged decoders, queues, uploads, and compositor pads.
- Explicit counters for applied plans, replaced branches, full-pipeline
  fallbacks, last changed count, elapsed time, and whether fallback was used.
- A counted full NULL rebuild fallback when a platform refuses the partial path.
- Clean branch teardown, including request-pad release, during replacement and
  shutdown.

## Explicitly excluded

- Layout geometry changes; a different tile count still rebuilds the complete
  grid because its compositor geometry changes.
- Frame-interpolated crossfades, dual-decoder pre-roll, RTP session migration,
  source failover, or proof of uninterrupted network transport.
- Per-camera RTSP validation without valid credentials, low-end i5 calibration,
  Playback pipeline changes, or changes to governor policy/capacity.
- Hiding fallback. A platform that requires a full restart remains functional,
  but its fallback counter makes the loss of continuity explicit.

## Acceptance

1. A same-layout plan rebuilds only branches whose tier changed.
2. Unchanged branches and their compositor pads are not destroyed.
3. Repeated H.265/D3D12 applies do not hang or emit a fatal bus error.
4. A supported dev-box run records zero full-pipeline fallbacks.
5. Active branches resume decoded output and capacity-paused branches remain
   honestly paused after repeated applies.
6. No-op plans do not increment applied-plan or branch-rebuild counters.
7. Layout changes retain the existing full-geometry rebuild behavior.
8. All workspace, native component, performance, and root regression gates
   remain green.

## Verification (2026-08-09)

- Release 4-tile H.265/D3D12 smoke: 3 applied plans, 6 branch replacements,
  zero full fallbacks, 4/4 playing at 25.9 FPS, 74 ms QML load.
- Release 16-tile smoke: 5 applied plans, 10 branch replacements, zero full
  fallbacks; apply time 21.9-25.5 ms; decoded output remained active.
- Release 64-tile low-end stress: 11 consecutive applied plans, 23 branch
  replacements, zero full fallbacks; first hardware transition 274.9 ms and
  subsequent applies 25.2-70.8 ms. At exit, 16 branches were playing at 29.2
  FPS, 46/46 capacity-paused branches were honest, and QML loaded in 82 ms.
- All 13 workspace gates and the root regression suite pass. Release spatial
  controller p95 is 0.0006 ms idle / 0.1265 ms active over 5,000 samples.
- Aggregate native components pass 10/11 with the unchanged intermittent ONVIF
  Windows loader failure; focused ONVIF passes immediately.
