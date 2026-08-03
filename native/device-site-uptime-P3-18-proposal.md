# P3-18 device-site ownership and reachability time (native increment 37)

Date: 2026-08-03  
Status: approved, built, and verified under the operator's directive to
identify and start the next build

## Decision

Make the premises operations panel genuinely site-scoped and replace its
uptime/last-seen placeholder with persisted reachability evidence. Devices gain
an optional canonical premises-site assignment. Positive and negative
reachability observations are persisted separately from the existing in-memory
multidimensional health model.

## Included

- Schema v13 optional `devices.site_id` ownership and one persisted
  reachability observation row per device.
- Observation fields: latest reach state and observation UTC, last positive
  reachability UTC, and the start UTC of the current uninterrupted positive
  reachability run.
- `DeviceRepo` assignment/read APIs and monotonic atomic reach-observation
  writes. Removing a device cascades its observation; deleting a site unassigns
  devices rather than deleting inventory.
- `device.site <id> <site|none>` through the existing audited
  `devices.manage` command envelope.
- Device read-model fields for assignment, persisted evidence, last seen, and
  current uptime availability/duration.
- Site operations device-health counts filtered to the active site, plus an
  explicit global unassigned count.
- A premises uptime/last-seen aggregate showing currently reachable devices,
  longest evidenced current uptime, and latest positive observation.
- Device cards display their site assignment or an explicit Unassigned state.

## Honesty and time rules

- Uptime means an uninterrupted run of positive **reachability** observations;
  it does not mean application runtime, stream health, storage health, or
  recording duration.
- `last_seen_utc` advances only on a positive reachability observation.
- A negative observation clears `online_since_utc` but preserves last seen.
- A later positive observation begins a new run. Repeated positives preserve
  the original run start.
- Older out-of-order observations are rejected atomically so clocks or delayed
  jobs cannot move evidence backward.
- After process restart, persisted timestamps are shown as history, but current
  uptime remains unavailable until this session has fresh positive reachability
  evidence.
- Unassigned devices are never silently counted under the active site.

## Explicitly excluded

- Automatic site inference from IP ranges, floor/camera placement, names, or
  the currently selected site; bulk assignment and a dedicated assignment UI.
- Availability SLA calculations, downtime history, flap history, retention,
  alerts based on duration, coordinator replication, or clock synchronization.
- Stream/recording cumulative duration and analytics, which remain later P3-18
  slices.

## Acceptance

1. Device assignment survives reopen; bad site/device ids fail without changing
   the previous assignment, and `none` explicitly unassigns.
2. Positive→positive preserves `online_since`; negative clears it while keeping
   last seen; a later positive starts a new run; stale observations are refused.
3. Restarted models expose last seen but do not claim current uptime until a
   fresh positive observation arrives.
4. The active-site aggregate excludes other-site and unassigned devices and
   reports the unassigned count explicitly.
5. Assignment uses the audited command envelope and appears in the catalog.
6. The panel renders evidenced uptime/last-seen, while a site with no evidence
   remains explicitly unavailable.
7. Persistence, QML/video smoke, performance, workspace, component, and root
   regression gates remain green.

## Verification (2026-08-03)

- `vms_dbtest`: 49/49 checks pass at schema v13, including assignment and bad
  assignment, positive repeat, offline reset, stale-observation refusal, new
  run, reopen, and exact timestamp persistence.
- `--devices-selftest`: 85/85 checks pass. `device.site` is cataloged and
  audited; current uptime advances across positive observations; restart keeps
  last seen and starts a new run on fresh evidence; offline preserves last seen;
  recovery starts a new run; device deletion cascades its observation.
- `--site-operations-selftest`: 26/26 checks pass. The active site includes
  only its two assigned devices, reports one unassigned device, supplies
  reachability uptime/last-seen, and excludes those devices after switching to
  another site.
- Release persistent offscreen video smoke: schema v13, QML load 92 ms, panel
  loaded, 6 site-assigned devices, 4 current reachable runs, persisted latest
  seen, 4 playing streams, and 26.4 FPS.
- Performance remains inside the P3-15 gate: Debug p95 0.0026/1.6001 ms and
  Release p95 0.0006/0.1322 ms for idle/active scenarios.
- All 13 workspace gates and the root regression suite pass. Aggregate native
  CTest passed 10/11 components; the unchanged intermittent ONVIF Windows
  loader failure (`0xc0000135`) passed immediately in a focused rerun.
