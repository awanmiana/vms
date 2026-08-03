# P3-18 operating hours and open-state slice (native increment 36)

Date: 2026-08-03  
Status: approved, built, and verified under the operator's directive to
identify and start the next build

## Decision

Replace the premises panel's operating-hours unavailable placeholder with a
persisted, site-keyed weekly schedule and local-date exceptions. Evaluate the
current open/closed state from a UTC instant through the site's persisted IANA
timezone. Configuration enters through the existing audited command envelope;
the front-layer panel remains read-only.

## Included

- Schema v12 schedule marker, weekly time windows, local-date exception, and
  exception-window tables, all owned by a canonical premises site.
- `PremisesRepo` atomic add/clear operations and schedule round-trip.
- Weekly windows use ISO weekdays 1–7 and half-open local intervals
  `[start_minute, end_minute)`. A day can contain multiple non-overlapping
  windows; `24:00` is valid only as an interval end.
- Date exceptions fully replace the weekly rule for that local date: either
  explicitly closed or one special opening window.
- Audited `premises.hours.add`, `premises.hours.clear`,
  `premises.hours.exception`, `premises.hours.holiday`, and
  `premises.hours.exception.clear` commands under `premises.manage`.
- Timezone-aware evaluation and an operator-facing Open/Closed state, rule
  source, and today's hours in the existing premises operations panel.
- Persistence, command, DST, aggregation, QML smoke, performance, component,
  and root regression verification.

## Time and DST semantics

- Stored windows are wall-clock minutes in the site's IANA timezone; they are
  not converted and frozen as UTC offsets.
- Evaluation always begins with an unambiguous UTC instant and converts it
  through Qt's timezone database. A spring-forward nonexistent wall interval
  therefore contains no instants. During fall-back, both occurrences of a
  repeated wall time follow the same configured local rule.
- The interval end is exclusive. A window ending at `17:00` is closed at
  exactly `17:00` local time.
- Overnight intent is represented explicitly as two windows split at
  midnight. This slice rejects `end <= start` rather than guessing which date
  owns an overnight interval.

## Honesty and safety rules

- No schedule marker means unavailable, not Closed.
- A configured date exception replaces the weekly day; no merging is implied.
- An invalid site timezone keeps open/closed unavailable with a reason.
- Weekly windows must not overlap. Invalid dates, days, clocks, empty site ids,
  or conflicting windows fail atomically.
- Schedule changes are presentation/configuration changes only; they do not
  start or stop cameras, recording, access control, or any physical operation.

## Explicitly excluded

- Calendar recurrence beyond weekly rules, multi-day intervals, sunrise/sunset
  rules, remote calendar import, locale-specific working-day assumptions, or
  cross-site inheritance.
- A schedule editor UI, role separation beyond the existing permission-ready
  capability, notifications, automation, recording control, or device control.
- Uptime/last-seen persistence, cumulative duration accounting, and analytics;
  those remain later P3-18 slices.

## Acceptance

1. A configured weekly window survives reopen and produces Open only inside
   its half-open local interval.
2. Closed and special-hours date exceptions override the weekly rule.
3. Invalid and overlapping inputs fail without partially changing the schedule.
4. UTC-to-site evaluation demonstrates spring-forward and repeated-hour DST
   behavior without manually fixed offsets.
5. The panel renders Open/Closed plus the governing rule and today's hours;
   an unconfigured schedule remains explicitly unavailable.
6. Schedule mutations are discoverable, capability-checked, audited commands.
7. Existing performance, workspace, component, and root regression gates stay
   green.

## Verification (2026-08-03)

- `vms_dbtest` reaches schema v12 and passes schedule migration, split-window
  round-trip across reopen, closed/special exception round-trip, and atomic
  overlap-refusal checks.
- `--site-operations-selftest`: 23/23 checks pass, including unconfigured
  honesty, half-open interval boundaries, closed and special-date replacement,
  immediate aggregate refresh, spring-forward behavior, and both occurrences
  of a repeated fall-back hour.
- `--command-selftest` passes with discoverable and audited hours commands,
  source mutation, and atomic overlap refusal through the command envelope.
- An authenticated loopback API call changed the running schedule; the Debug
  offscreen scene then reported `hours=Open`, `panel=loaded`, and 4/4 playing
  streams. Release restored the same persisted schedule, loaded QML in 97 ms,
  and reported `hours=Open`, `panel=loaded`, 4/4 playing, and 24.0 FPS.
- Performance remains inside the P3-15 gate: Debug p95 0.0026/1.7772 ms and
  Release p95 0.0006/0.1680 ms for idle/active scenarios.
- All 13 workspace gates and the root regression suite pass. Aggregate native
  CTest passed 10/11 components; the unchanged intermittent ONVIF Windows
  loader failure (`0xc0000135`) passed immediately in a focused rerun.
