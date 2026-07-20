# Media Policy: Bandwidth, Streams, and Lockout Prevention

> **Document role:** Prototype policy reference, not a second roadmap. Feature scope, order, status, production limits, and verification checks live in [`../Development_plan.md`](../Development_plan.md), especially C0-05, P0-05 through P0-13, P3-03, and P12-03.

The current prototype chooses stream quality from screen role and configured device limits rather than fixed camera settings. The tiers and bitrate ranges below are provisional until supported workstation, codec, adapter, and measured capacity profiles are approved.

## Activity Priority Is Separate

P0-01 approves future device/work activity states such as `high`, `medium`, `low`,
and `idle`. They will control whether a device receives expensive connections,
polling, event work, playback/download work, or streaming attention. Their exact
transitions and budgets are not implemented or approved yet.

The `main`, `sub`, `thumb`, and `paused` values below are different: they select
media quality for a particular view/session. A future high-priority device can
still have several view tiers, while an idle device must not retain a continuous
stream merely because a previous view used `main`.

## Stream Tiers

- `thumb`: large grids and previews. Target 128-384 kbps.
- `sub`: normal grid monitoring. Target 512-1500 kbps.
- `main`: focus view, tracking target, evidence export.
- `paused`: off-screen or deprioritized stream.

## Tier Decision

```js
resolveTier({
  tileCount,
  isFocused,
  isTracking,
  paneContext,
  isVisible
})
```

Rules:

- Hidden/off-screen: `paused`
- Tracking camera: `main`
- Focus/single view: `main`
- Playback scrubbing/multi-compare: `sub`
- Grid 9 or more: `thumb`
- Grid 2 to 8: `sub`
- Grid 1: `main`

## Budget Guard

Before a real media bridge opens a stream:

1. Check operator session bandwidth budget.
2. Check per-device `max_concurrent_mainstream`.
3. Check per-device `max_concurrent_substream`.
4. Downgrade the lowest-priority tile before exceeding a limit.
5. Never hammer an NVR/DVR with repeated failed credential/session attempts.

## Tracking Mode

Tracking mode should:

- Pin focused camera to `main`.
- Pre-warm next/previous route cameras at `sub`.
- Downgrade unrelated tiles to `thumb` or `paused`.
- Restore the previous layout when tracking ends.

## Roadmap Dependencies

Production persistence remains P0-04 work; shared policy consolidation is C0-05; real live media and stream-profile import require P0-14/P2-14/P3 approval; recorder playback remains P5 work; tracking pre-warm depends on an approved and verified live-stream path. These are dependencies, not an independent implementation order.
