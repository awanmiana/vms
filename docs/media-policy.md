# Media Policy: Bandwidth, Streams, and Lockout Prevention

The app must choose stream quality from screen role and device limits, not from fixed camera settings.

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

## Build Order

This is dependency order, not implementation authorization:

1. Approve production persistence and schema ownership.
2. Maintain the media policy decision engine.
3. Maintain the device concurrency guard.
4. Add one-camera live preview only through an approved adapter/protocol bridge.
5. Import stream profiles only after the applicable standard/vendor evidence gate.
6. Add recorder playback through its separately approved adapter operation.
7. Add tracking pre-warm after live-stream behavior is verified.
