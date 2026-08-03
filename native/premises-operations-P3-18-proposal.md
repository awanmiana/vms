# P3-18 premises operations panel foundation (native increment 35)

Date: 2026-08-03  
Status: approved, built, and verified under the operator's directive to
identify and start the next build

## Decision

Build the first front-layer panel keyed by the active premises/site from data
the standalone runtime already owns. Every required field without an approved
source is present as typed unavailable with a reason; no operational or
analytical value is inferred.

## Included

- A read-only `SiteOperationsController` aggregating the active premises,
  device-health model, and Live media-diagnostics model.
- Site-local date/time derived from the persisted IANA timezone through Qt's
  timezone database, including an invalid-timezone state.
- Device inventory and honest health counts: online, degraded, offline,
  unknown, unsupported, detached, maintenance, and needs-attention.
- Governed media counts: playing, connecting, stalled, policy-paused, and
  unavailable, plus observed display FPS when present.
- Explicit unavailable contracts for operating hours/open state, device online
  uptime/last-seen, cumulative recording/streaming duration, and analysis data.
- A toggleable premises operations panel on the workspace front layer.
- Headless aggregation self-test and persistent offscreen QML/video smoke.

## Honesty rules

- Site time is available only when the persisted timezone is valid.
- Unknown device health remains unknown and is never counted online.
- A console/session duration is not substituted for device uptime or cumulative
  operating duration.
- Alarm counts are not relabeled as analytics. Until an approved analytics
  source exists, analysis remains unavailable.
- Operating hours/open state remain unavailable until a persisted schedule and
  its timezone/DST semantics are approved.

## Explicitly excluded

- Operating-hours configuration/schema, holiday calendars, schedule commands,
  uptime/last-seen persistence, cumulative-duration accounting, analytics/VCA
  ingestion, trends, reports, alerts, or historical storage.
- Cross-site administration, multi-site rollups, remote coordinators, cloud
  services, new device polling, or media transport changes.
- Any mutation from the panel; it is read-only and adds no command-envelope
  bypass.

## Acceptance

1. The active site/floor/timezone and valid site-local clock appear in the
   front-layer panel.
2. Device and media aggregates exactly match their source models, preserving
   unknown/paused/unavailable distinctions.
3. Operating hours, last-seen/uptime, cumulative duration, and analysis render
   explicit unavailable states with reasons.
4. Source updates refresh the panel without polling or mutating those sources;
   only the local clock ticks once per second.
5. Selftest, persistent QML/video smoke, performance, workspace, component, and
   root regression gates remain green.

## Verification (2026-08-03)

- `--site-operations-selftest`: 15/15 checks pass, including valid IANA local
  time, source-accurate device/media aggregates, explicit unavailable reasons,
  and immediate refresh from source signals.
- Release persistent offscreen smoke (`--devices-demo --count 4 --profile
  lowend --video --sweep-interval 0 --smoke-ms 5000`): QML loaded in 103 ms;
  the panel loaded on the front layer with the Default site in UTC, 6 devices
  (3 online, 1 degraded, 1 offline), 4 playing streams, and observed 24.6 FPS.
- The spatial performance gate remains inside budget: Debug p95
  0.0039/2.3720 ms and Release p95 0.0010/0.2185 ms for idle/active scenarios.
- All 13 workspace selftest gates and the root regression suite pass. Aggregate
  native CTest passed 10/11 components; the unchanged intermittent Windows
  ONVIF loader failure (`0xc0000135`) passed immediately in a focused rerun.
