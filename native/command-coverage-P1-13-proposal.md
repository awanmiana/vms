# Command coverage — P1-13 second slice (A0 mandate), native pass

> Scope note for native increment 29. Subordinate to `Development_plan.md`.
> Proceeding under the owner's standing "build the next thing, as you
> recommend" directive (2026-07-31, repeated). Closes the coverage half of
> P1-13, which increments 25–28 each deferred here.

## Problem & users

A0 requires that **every** state-changing action be one validated command, and
P1-13 names the enforcement: *"a coverage check that fails if any UI action
bypasses the command layer."* Today the envelope exists (inc 25), is reachable
externally (inc 26) and durably audited (inc 28) — but the UI still calls
controllers directly at **27 sites**; only the alarm panel routes through it.
Every bypassed click is an action with no capability check, no confirmation
gate, and no audit row.

## Included scope (this pass)

- **A structured invocation path for QML**: `CommandController::invoke(id,
  argsMap, confirm)` returning `{outcome, message, ok}`, so a click with typed
  arguments (names with spaces, URLs, credentials) routes through the same gate
  without the text leg's whitespace tokenization.
- **Commands for the remaining discrete operator verbs**, so the UI has
  something to route *to*:
  - devices (the destructive/config surface where audit matters most):
    `device.onboard`, `device.onboardRecorder`, `device.onboardDiscovered`,
    `device.discover`, `device.rescan`, `device.acknowledge`,
    `device.maintenance`, `channel.rename`, `channel.disable`,
    `channel.remove` (dangerous), `channel.move`;
  - workspace: `workspace.place` (the committed tile drag).
- **Routing**: every declared mutator call site in `Workspace.qml` and
  `DevicesView.qml` goes through `commander.invoke(...)`/`run(...)` when the
  envelope is present, with a direct fallback only when it is absent (a build
  without persistence).
- **The coverage check itself** — a pure `CoverageCheck` (`vms_command`) that
  scans the **shipped** QML (read from the Qt resource, so it checks what
  actually ships, not a stale source path) for `controller.method(` call sites,
  classifies each against a declared table of **state-changing mutators**, and
  reports a violation for any mutator invoked directly rather than through the
  envelope. Exposed as `--coverage-check` (prints the report; **non-zero exit
  on any violation**, so it is CI-gateable) and covered by `--coverage-selftest`
  (including a negative test: an injected bypass **must** be detected).
- **Documented, justified exemptions** — recorded in the check itself, not
  hidden:
  - *pure reads* (e.g. `zoomLevelName`) — reading changes no state;
  - *continuous viewport recomputation* (`updateSpatialViewport`) — derived
    policy recomputed on every pan/zoom frame, persists nothing; the committed
    result of the gesture (`workspace.place`) **is** a command;
  - *continuous transport scrubbing* (`seekFrac` while dragging the playhead) —
    the discrete transport verbs remain commands.

## Excluded (own later gates)

- Voice invocation (P1-12's remaining leg).
- Playback transport commands beyond the existing `replay.*` (the Playback tab's
  own transport is its own small slice).
- Auth/roles behind the capability strings (P1-02/P1-03) — the Administrator
  session still grants all; the gate already checks membership.
- Automatic enforcement at compile time (the check is a test/CI gate, not a
  build-breaking lint).

## Acceptance criteria (headless)

1. Every declared mutator in the shipped QML is invoked through the envelope;
   `--coverage-check` reports zero violations and exits 0.
2. The check **detects** an injected direct-call bypass (negative test) and
   exits non-zero.
3. Each exemption is explicit, categorized, and printed in the report — no
   silent skips.
4. Device actions invoked from the UI produce durable audit rows with their
   arguments (inc 28), and `channel.remove` refuses without confirmation.
5. All prior self-tests stay green.
