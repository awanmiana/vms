# P3-18 cumulative recording duration (native increment 38)

Date: 2026-08-03  
Status: approved, built, and verified under the operator's directive to
identify and start the next build

## Decision

Replace the premises panel's cumulative-duration placeholder with an honest,
site-scoped total derived from completed rows in the existing local recording
`SegmentIndex`. Keep streaming duration separate and explicitly unavailable:
the current live diagnostics are observations, not a persistent lifecycle log,
so they cannot support cumulative stream time across restarts.

## Included

- A Qt-free `SegmentIndex::recordedDuration` aggregate for a set of stable camera
  ids.
- Same-camera segment intervals are unioned before summing, so duplicate or
  overlapping fragments do not inflate recorded time. Durations from different
  cameras are summed as camera-time.
- Canonical UTC validation for newly indexed completed segments. Malformed
  legacy rows are excluded from duration and counted explicitly.
- Active-site ownership is resolved through the current device/channel model;
  other-site, unassigned, and orphan segment ids are excluded.
- A successful query with assigned cameras and no completed footage reports an
  honest zero. Missing source/site/cameras and query failure remain reasoned
  unavailable states.
- The front-layer panel shows recorded duration, completed segment/camera
  evidence, removed overlap, and an explicit streaming-unavailable qualifier.
- Duration refresh is cached for 30 seconds so the one-second site clock does
  not repeatedly scan recording history; ownership changes invalidate the scope
  naturally through its stable camera-id key.

## Ownership and history rule

The segment table deliberately outlives inventory and has no camera foreign
key. This slice attributes footage using **current** device-to-site ownership.
It does not claim historical site ownership at recording time. Orphan footage
remains retained/playable by its camera id but is not silently attributed to a
premises.

## Explicitly excluded

- Persistent stream-session start/stop accounting, stream duration across
  restart, remote/NVR/cloud recording indices, or cross-authority reconciliation.
- Wall-clock elapsed time spanning recording gaps, schedule-open duration,
  retention forecasts, storage utilization, SLA/downtime, or analytics.
- Historical ownership snapshots and retroactive attribution after a camera is
  moved between sites.

## Acceptance

1. Cumulative time unions overlaps per camera and sums time across cameras.
2. Duplicate camera ids do not double-count; invalid legacy rows are reported
   and excluded; newly indexed malformed timestamps are refused.
3. Only current active-site camera ids contribute, including completed footage
   for disabled channels that remain in inventory.
4. A connected index with no completed footage returns zero, while absent
   source/site/cameras or query failure remains explicitly unavailable.
5. The panel exposes evidence counts and never presents the unavailable
   streaming component as zero.
6. Recording, site-operations, QML/video smoke, performance, workspace,
   component, and root regression gates remain green.

## Verification (2026-08-03)

- `vms_rectest` passes canonical-calendar rejection, camera-id deduplication,
  same-camera overlap union, cross-camera summation, zero-footage, and malformed
  legacy-row exclusion checks.
- `--site-operations-selftest` passes 29/29 checks, including 1,200 seconds of
  active-site camera-time from 1,320 raw seconds, unassigned and site-switch
  exclusion, retained disabled-channel history, and explicit streaming
  unavailability.
- Release end-to-end record→index→panel smoke reports 5 recorded seconds across
  2 completed segments, QML load 91 ms, 4 playing streams, and 26.1 FPS.
- Spatial performance remains within budget: Debug p95 0.0026/1.7340 ms and
  Release p95 0.0006/0.1120 ms for idle/active scenarios.
- Debug and Release builds, all 13 workspace gates, and the root regression
  suite pass. Aggregate native CTest passes 10/11 components; the unchanged
  intermittent ONVIF Windows loader failure (`0xc0000135`) passes immediately
  in a focused rerun.
