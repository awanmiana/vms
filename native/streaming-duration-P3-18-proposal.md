# P3-18 persistent streaming duration (native increment 39)

Date: 2026-08-05  
Status: approved, built, and verified under the operator's directive to
identify and start the next build

## Decision

Complete the remaining operations-time source in P3-18 by persistently
checkpointing **observed decoded branch-time** for the active premises. Count
only branches whose live diagnostics report `playing`; connecting, stalled,
paused, and unavailable branches contribute no time. This is camera-time (two
playing branches for one second add two stream-seconds), not wall-clock site
open time or proof of source-device transmission.

## Included

- Schema v14 storage keyed by stable premises id, holding integer observed
  stream milliseconds, checkpoint count, and the last checkpoint time.
- A Qt-free repository that atomically adds a non-negative elapsed interval
  multiplied by its observed playing-branch count, with overflow protection.
- Monotonic in-process elapsed measurement in the premises operations
  controller. UTC is stored only as checkpoint evidence and is never used to
  infer elapsed time.
- Site-switch isolation: elapsed time is settled to the site that owned the
  observation before the next active site begins accumulating.
- Restart honesty: persisted totals survive restart, but no interval while the
  process was stopped is invented. A crash may lose the uncheckpointed fraction
  since the previous refresh; it can never add offline time.
- The front-layer panel reports recording and streaming duration separately,
  plus a combined cumulative camera-time only when both sources are available.
- Typed unavailable/error state when the streaming store cannot be queried or
  updated. A connected source with no observations reports an honest zero.

## Source and attribution rule

The current governed Live workspace is scoped by the active premises but its
synthetic tile ids are not stable inventory camera ids. Therefore this slice
attributes observed branch-time to the active premises and does not claim
per-camera stream history. The existing recording duration remains based on
stable camera ids and current device-to-site ownership. Those two evidence
sources stay visibly distinct.

## Explicitly excluded

- Inferring stream time from configured tiles, `connecting`, `stalled`, or
  policy-paused state; filling process downtime; transport/RTP session time;
  NVR/cloud/client reconciliation; or per-camera history without a stable media
  binding.
- Historical premises ownership snapshots, synchronized multi-client totals,
  retention/storage forecasting, bandwidth accounting, SLA calculations, and
  analytics fields.
- Treating the synthetic media harness as real-camera or network evidence.

## Acceptance

1. Multiple playing branches accumulate camera-time multiplicatively; zero
   playing branches add zero.
2. Negative elapsed/count values and arithmetic overflow are refused without a
   partial write.
3. Totals persist across reopen and site ids remain isolated.
4. Switching the active premises settles elapsed time to the previous site and
   does not backfill time across restart.
5. The panel reports persisted streaming evidence separately from recording
   evidence and never relabels connecting/stalled/paused time as streaming.
6. Persistence, site-operations, QML/video smoke, performance, native component,
   and root regression gates remain green.

## Verification (2026-08-05)

- `vms_dbtest` passes 54/54 checks at schema v14, including honest zero,
  multiplicative branch-time, millisecond preservation, negative/overflow
  refusal, checkpoint evidence, and reopen durability without downtime fill.
- `--site-operations-selftest` passes 30/30 checks, including separate recorded
  and streamed evidence, combined camera-time, site switching, and continued
  analytics unavailability.
- Two Release persistent offscreen video runs over the same database observed
  the total advance from 18,708 ms / 14 checkpoints to 29,248 ms / 22
  checkpoints. The second run loaded QML in 93 ms with four playing branches at
  25.6 FPS; process downtime was not added.
- Release spatial performance remains within budget: p95 0.0006 ms idle and
  0.1377 ms active over 5,000 samples.
- Debug and Release builds, all 13 workspace gates, root regressions, and 10/11
  aggregate native components pass. The unchanged intermittent ONVIF Windows
  loader failure (`0xc0000135`) passes immediately in focused Debug and Release
  CTest runs.
