# Event rules & alarm lifecycle — P6-03/P6-07 first slice, native pass

> Scope note for native increment 27. Subordinate to `Development_plan.md`.
> Proceeding under the owner's standing "build the next thing, as you
> recommend" directive (2026-07-31, repeated).

## Problem & users

The operator needs one honest alarm surface: things that happen (a device goes
offline, a stream dies, storage degrades — later: motion, analytics, inputs)
must become **events**, events must pass a **rule engine** deciding what
deserves an alarm at what priority, and alarms must live an accountable
**lifecycle** (new → acknowledged → escalated → cleared) with deduplication and
maintenance suppression — not a scrolling log the operator tunes out.

## Included scope (this pass)

- A pure, Qt-free **`AlarmEngine`** (`src/events/`, lib `vms_events`,
  `vms_eventtest`/CTest), in the same decision-core style as `HealthMonitor`:
  - normalized `Event` {type, deviceId, severity, message, time};
  - **rules** (P6-03 first pass): trigger = event-type match + optional device
    filter; enabled/disabled; priority mapping; an optional **auto-clear
    event type** (e.g. `device-recovered` clears `device-offline` alarms);
  - **alarm lifecycle** (P6-07): raise → **dedup** (a matching active alarm
    bumps count + last-seen instead of duplicating; an acknowledged alarm
    stays acknowledged on re-ingest, honest to the HealthMonitor pattern) →
    acknowledge (silences, persists) → escalate (re-raises attention) →
    clear (manual or auto on the recovery event); a cleared alarm's
    re-occurrence is a **fresh** alarm;
  - **maintenance suppression** per device (suppresses notification, never the
    state) and honest `needsAttention()` / `shouldNotify()`;
  - injectable time (the caller stamps events), no clock dependency.
- **First event source:** device health (P6-08's normalization direction) —
  `DeviceController` emits `healthTransition` events (offline / degraded /
  recovered) from its existing live health feed, which the app routes into the
  engine under three default rules (offline→High, degraded→Medium, both
  auto-cleared by recovery).
- An **`AlarmController`** (Qt) binding the engine to QML, and an **Alarms
  panel** in the workspace (toolbar chip with a needs-attention badge; layered
  list with per-alarm Acknowledge / Escalate / Clear).
- **A0 wiring:** `alarm.ack` / `alarm.escalate` / `alarm.clear` registered in
  the command envelope — palette- and API-invokable immediately; the panel's
  buttons invoke **through the envelope** when it is present (the first UI
  surface natively routed through commands, the P1-13 coverage direction).
- `--alarms-selftest` + offscreen smoke.

## Excluded (own later gates)

- **Automated linkage/actions** (camera pop-up, PTZ, outputs, dispatch…) —
  P6-05 is separately approved; this engine **notifies, never actuates**.
- Durable event/alarm storage, backpressure, ordering, and the measured
  throughput profile (P6-04); rule schedules/correlation/simulation/versioning
  (rest of P6-03); delivery channels/SLO (rest of P6-07); ARC (P6-09/10).
- Event sources beyond device health (motion/VCA/inputs — their own gates).

## Acceptance criteria (headless)

1. Rule matching: type + device filter; a disabled rule and an unmatched event
   raise nothing.
2. Dedup: re-ingest bumps count/last-seen on the active alarm (acknowledged
   stays acknowledged); after clear, a re-occurrence is a fresh alarm.
3. Lifecycle: acknowledge silences but persists; escalate re-raises attention;
   manual clear and **auto-clear on the recovery event** both work.
4. Maintenance suppresses notification, never the alarm state.
5. A live health transition (offline) raises the High alarm in the UI model and
   `alarm.ack <id>` through the **envelope** acknowledges it (audited).
6. Offscreen smoke loads the Alarms panel with no QML errors; all prior
   self-tests stay green.
