# Advanced event rules and alarm delivery - P6-03/P6-07 completion slice

> Scope note for native increment 45. Subordinate to `Development_plan.md`.
> Approved by the operator's directive to complete every approved partial
> native capability using worldwide-standard behavior.

## Included

- Versioned rules with monotonic replacement and fail-closed validation.
- Explicit ISO-8601 weekday schedules expressed in UTC minutes, including
  overnight windows, with no host-local/DST ambiguity.
- Normalized exact-match event conditions, same-device time-window
  correlation, active-alarm dependencies, and side-effect-free simulation
  with reason codes.
- Independent evaluation counters and a 1,000-active-rule measured profile.
- Alarm assignment, acknowledgement and resolution deadlines, breach state,
  and escalation on missed acknowledgement without false auto-clear.
- Connector-neutral SMTP/SMS/webhook-style delivery jobs with stable
  idempotency keys, bounded exponential retries, delivered/exhausted/cancelled
  states, maintenance suppression, and audit callbacks for automatic and
  operator lifecycle changes.
- `alarm.assign` through the same validated, capability-checked, audited A0
  command envelope as the existing lifecycle verbs.

## Standards and safety boundary

UTC schedule semantics follow ISO 8601 weekday/time conventions. Delivery
retry is bounded and idempotency-aware in the spirit of HTTP safe retry
practice; connectors are injected and own protocol-specific authentication,
TLS, acknowledgement, and provider receipts. The engine only notifies. It
does not actuate cameras, outputs, PTZ, dispatch, or emergency services.

No SMTP, SMS, or webhook provider is bundled or claimed operational without
the operator selecting and authorizing one. Durable alarm/event queues remain
P6-04; ARC protocols remain P6-09/P6-10.

## Verification

`vms_eventtest` covers legacy matching/lifecycle plus version refusal,
weekday and overnight schedules, conditions, correlation isolation/expiry,
dependencies, simulation non-mutation, assignments, SLA/SLO breaches,
connector retry/exhaustion, idempotency keys, maintenance suppression, audit
callbacks, and the measured 1,000-rule profile. `--alarms-selftest` verifies
that `alarm.assign` changes the real QML-facing model through the envelope.

