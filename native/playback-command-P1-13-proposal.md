# Playback Transport Commands — P1-13 / A0 (Native Increment 30)

## Decision

Build the explicitly deferred command-coverage slice for the existing recorded
Playback tab and instant-replay overlay. This is a completion of the approved A0
command-first mandate, not a new media or recording feature.

## Problem and users

The Playback UI already had working play, pause, speed, frame-step, and seek
controls, but those buttons called `PlaybackController` directly. That left the
last named discrete mutation gap in the shipped QML: a human click could change
transport state without the same capability check, deterministic result, API
surface, and durable audit used by the palette and external clients.

Operators need the existing controls to behave unchanged. Administrators and
external clients need the same verbs to be discoverable, permission-ready, and
auditable through one envelope.

## Included scope

- Add recorded-Playback commands: `playback.play`, `playback.pause`,
  `playback.speed`, `playback.step`, and `playback.seek`.
- Add instant-replay transport commands: `replay.play`, `replay.pause`,
  `replay.speed`, and `replay.seek`.
- Use the existing `playback.review` capability on every new command.
- Route every discrete Playback and instant-replay transport button through the
  structured command path used by QML and the external API.
- Keep high-frequency scrub movement as a declared continuous-input exemption,
  while committing the final seek through the envelope when the drag releases.
- Refuse honestly when no recording DB or no active instant replay with footage
  is available.
- Tighten the coverage gate so a future direct play/pause/speed/frame-step call
  is a violation, and remove all `DeferredSlice` exemptions from the shipped
  mutator table.

## Excluded scope

- No new footage source, codec, recorder retrieval, synchronized playback, or
  evidence workflow.
- No transport hardening (scoped tokens, TLS, rate limits, streaming) and no
  non-loopback API exposure.
- No RBAC implementation; the Administrator session continues to grant every
  registered capability.
- No audit row for every intermediate mouse-move during a scrub. Only the
  committed seek is audited, avoiding an unbounded event stream.

## Data and security impact

The slice adds no schema or secret storage. Every discrete transport attempt is
subject to the existing validation, `playback.review` capability check, result
mapping, session audit, and durable hash-chained audit when the store is open.
Seek and speed values are range-validated (`0..1` and `0.25..16`).

## Acceptance

1. The command catalog exposes all nine transport commands with stable parameter
   types, numeric ranges, and `playback.review` capability metadata.
2. Commands drive the real `PlaybackController` state for play, pause, speed,
   frame-step, and seek; absence of a controller or active replay fails honestly.
3. All discrete transport buttons use `commander.invoke`; drag movement remains
   responsive and its release commits an audited seek.
4. `--coverage-check` reports zero violations and no `DeferredSlice` exemption;
   an injected direct transport mutation would fail the same gate.
5. Command, Playback, instant-replay, API, and coverage self-tests pass; all prior
   workspace gates and the offscreen QML smoke remain green.

## Result (2026-08-01)

Built and verified on the development box. The nine commands are registered and
API-discoverable; shipped QML routes all discrete transport controls through the
envelope; the only remaining direct calls are the four declared in-progress
scrub-frame sites, each paired with a command-routed release. `--coverage-check`
reports 0 violations across 39 declared mutators and 7 exempt direct sites, with
no deferred-slice exemption. All ten workspace gates, the root regression suite,
the native build, and the offscreen smoke pass. The 11 native component tests all
pass; in a single aggregate CTest invocation the unchanged ONVIF test can hit a
transient Windows DLL-loader `0xc0000135`, and passes immediately in its focused
CTest run.
