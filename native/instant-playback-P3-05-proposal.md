# Instant playback from live view — P3-05, first standalone pass

> Scope note for native increment 23. Subordinate to `Development_plan.md`
> (the controlling authority). Built under the owner's standing "build the next
> thing, as you recommend, standards-compliant" directive; the owner selected
> this lane on 2026-07-31.

## Problem & users

An operator watching the Live wall sees something happen and wants to review the
**last few seconds immediately** — without leaving Live, opening the Playback
tab, and constructing a start/end range. P3-05 ("Instant playback from live
view") is the standard VMS "instant replay" verb: jump back N seconds on the
selected camera, watch, then return to live.

## Benefit

One click turns "I think I just saw something" into a replay of the moment,
using footage the deployer is already recording locally — the fastest possible
review loop, with no query workflow.

## Included scope (this pass)

- A pure-ish `InstantReplayController` (Qt + `vms_persist`) that, for a camera
  with a local recording, computes the **live edge** (latest recorded moment)
  and offers `replay(seconds)`: it opens a look-back window `[edge − seconds,
  edge]`, positions the playhead at the window start, and auto-plays — **reusing
  the existing `PlaybackController` (timeline/availability/transport) and
  `PlaybackPipeline` (decode)**, no duplication.
- **Honest states (P0-03):**
  - a camera with **no local recording** → the overlay opens in an *unavailable*
    state ("No local recording for this camera"); it never presents requested
    time as recorded and fabricates no availability spans;
  - a look-back **longer than the available footage** clamps the window start to
    the earliest recorded moment and reports the *actual* look-back, not the
    requested one.
- **UI:** an "Instant replay · 10s / 30s / 60s" control on the Live selected-
  camera info panel, and a layered replay overlay over the Live grid (P3-14
  layering — the live wall keeps running behind it) with the decoded replay
  video, a compact transport (scrub / play-pause / speed, bound to the reused
  `PlaybackController`), the honest availability bar, and a **"⟵ Live"** return
  control.
- Routed through an `Q_INVOKABLE` controller (`replay` / `returnToLive`) so it
  can later become an A0 command (P1-12/P1-13) without rework.
- Verified offscreen: a `--instant-selftest` (controller logic, no display) plus
  the existing offscreen QML smoke exercising the overlay + a real decode.

## Excluded (own later gates)

- Mapping an **arbitrary** live tile to a recorded `camera_id` — the live media↔
  inventory binding is a later media gate; this pass replays the recording-backed
  camera (`--rec-camera`), the camera that actually has local footage.
- **Recorder/NVR/ONVIF-replay** retrieval of instant footage (P4-03/P2-14), cloud
  (P4-07/08), and multi-camera synchronized instant replay (P5-01).
- Look-back **permissions / RBAC** gating (P1-02/P1-03) and evidence capture
  (P5-08/09) — instant replay is read-only here.
- A configurable default look-back duration and per-camera pre-roll buffer
  (P3-05 refinements) beyond the fixed 10/30/60 s presets.

## Dependencies (all already built)

- Recording backend + segment index (increment 7a/7b, `SegmentIndex`).
- `PlaybackController` (7c-1) and `PlaybackPipeline` (7c-3).
- The Live workspace + layered info panel (P3-14 slices 1–8).

## Data & security impact

Read-only over the **local** recording index (P0-01E — optional, deployer-
located recording; no central archive). No new secrets, no new tables, no schema
change. Honest per-P0-03 states throughout.

## Acceptance criteria (measurable, offscreen-verifiable)

1. On a camera with local recordings, `replay(30)` activates, opens a window
   ending at the live edge and starting 30 s before it, positions the playhead
   **on recorded footage**, resolves a backing `.mp4`, and auto-plays.
2. `replay(N)` with `N` longer than the available footage clamps the window start
   to the earliest recorded moment and reports the **actual** look-back.
3. `replay()` on a camera with **no** local recording activates in an *unavailable*
   state with no fabricated spans and the playhead **not** on footage.
4. `returnToLive()` deactivates the overlay and pauses the decode.
5. The offscreen QML smoke loads the Live tab + replay overlay with **no QML
   errors/warnings**, and (GStreamer build) the replay decode pulls real frames.

## Alternatives considered

- **Extend `PlaybackController` with a `replay()` verb** instead of a new
  controller: rejected — it would entangle the Playback-tab timeline state with
  the Live overlay state (they must stay independent, per P3-14). Composition
  (the instant controller *owns* a `PlaybackController`) keeps both honest.
- **A ring buffer of decoded live frames** for zero-storage instant replay:
  deferred — it duplicates decode/memory the Quality Bar forbids and only works
  while a tile is decoding; the recorded-footage source is honest about what
  actually exists and reuses the shipped recording backend.
