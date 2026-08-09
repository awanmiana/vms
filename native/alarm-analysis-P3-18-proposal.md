# P3-18 site-scoped active alarm analysis (native increment 40)

Date: 2026-08-09  
Status: approved, built, and verified under the operator's directive to
identify what is next and continue building

## Decision

Close the standalone P3-18 premises panel with a narrow, source-honest
analysis summary over the native alarm engine. The alarm engine is the
authority for current active device-health alarms; the device inventory is the
authority for current device-to-site ownership. The panel joins those read
models without creating a second event or ownership authority.

## Included

- Active alarms for devices currently assigned to the active premises.
- Counts for alarms needing attention, High/Medium/Low priority,
  acknowledged and maintenance-suppressed alarms, total deduplicated
  occurrences, and affected devices.
- Immediate refresh when an alarm is raised, deduplicated, acknowledged,
  escalated, cleared, recovered, or suppressed.
- Site-switch isolation. Alarms for another site, unassigned devices, system
  events without a device, and removed devices are excluded from the active
  premises summary entirely.
- An honest zero when the connected source has no current alarms.
- A visible session-only label so current state is not mistaken for retained
  history or a trend.

## Authority and attribution

This slice reads `AlarmController`/`AlarmEngine` for alarm lifecycle and
`DeviceController` for current site ownership. Attribution follows the current
inventory assignment, matching the other standalone P3-18 aggregates. It does
not reconstruct historical ownership.

## Explicitly excluded

- Video analytics/VCA detections, object classification, confidence scores,
  heat maps, dwell/occupancy, re-identification, face/plate recognition, or
  predictive claims.
- Durable event/alarm history, historical trends, rates, SLA calculations,
  cross-site rollups, correlation, reporting, export, or retention changes.
- Automatic actions, camera pop-ups, PTZ, dispatch, notification delivery, or
  any mutation from the read-only premises panel.
- Relabeling device-health alarms as proof of a video-analytics event.

Future VCA sources and retained event analytics remain separately gated by
their P3/P5/P6 policy, privacy, authority, and retention decisions.

## Acceptance

1. Only alarms whose current device ownership matches the active premises are
   counted.
2. Alarm priority, attention, acknowledgement, suppression, occurrence, and
   affected-device totals match the alarm engine exactly.
3. Alarm lifecycle changes refresh the panel immediately.
4. Switching premises cannot leak prior-site or unassigned alarm counts.
5. A connected empty source reports an available zero; a missing source reports
   a typed unavailable reason.
6. The UI identifies the summary as current-session device-health alarms and
   does not claim VCA or retained history.
7. Native component, workspace, QML smoke, performance, and root regression
   gates remain green.

## Verification (2026-08-09)

- `--site-operations-selftest` passes 32/32 checks, including a typed missing-
  source state, two active-site
  alarms, one excluded unassigned alarm, severity/occurrence totals,
  acknowledgement refresh, and zero leakage after a site switch.
- Release offscreen QML smoke loads in 160 ms and reports a valid available
  zero-alarm premises summary with the panel loaded.
- All 13 workspace gates and the root regression suite pass.
- Release 64-tile performance remains within budget: p95 0.0009 ms idle and
  0.1717 ms active over 5,000 samples.
- The Release native component suite passes 10/11 in aggregate with the
  unchanged intermittent ONVIF Windows loader failure; focused ONVIF passes
  immediately.
