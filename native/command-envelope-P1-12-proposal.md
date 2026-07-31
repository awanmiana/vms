# Unified command envelope — P1-12 first slice (A0 mandate), native pass

> Scope note for native increment 25. Subordinate to `Development_plan.md`.
> Proceeding under the owner's standing "build the next thing, as you
> recommend, standards-compliant" directive (2026-07-31: "what is there to
> build next, start building"). Realizes the first buildable slice of the
> owner's A0 mandate (2026-07-24) and P1-12.

## Problem & users

A0 requires every action the software performs to be **one validated command**
— invokable by UI, voice, and text, and eventually by an external AI agent
(P1-13) — with capability checks, confirmation of dangerous actions,
deterministic result mapping, and audit. Today the native workspace has ~a
dozen operator verbs (focus, priority, quality, layout, sweep, spatial, instant
replay, device rename/detach/remove…) that are only reachable as direct
controller calls from QML. Each new increment without the envelope deepens the
retrofit.

## Included scope (this pass)

- A **pure, Qt-free command core** (`src/command/`, lib `vms_command`,
  `vms_cmdtest`/CTest):
  - `CommandSpec` — stable id, title, **capability** string, `dangerous` flag,
    typed parameter specs (int/number/string/enum with required + range);
  - a `CommandRegistry` that **validates** an invocation against the spec
    (unknown command / missing param / bad type / out-of-range / bad enum →
    honest typed refusals), enforces **capability** membership, gates
    `dangerous` commands behind an explicit **confirm**, executes the handler,
    and maps the outcome to one deterministic `CommandResult`;
  - an **audit sink** invoked for every attempt — refused or executed — with
    the command id, arguments, and outcome (durable audit storage remains
    P1-06; this pass keeps an in-memory session log and stdout);
  - a deterministic **text parser** (`"focus 5"`, `"quality 3 thumb"`,
    `"device.remove cam-1 confirm"`): positional args by declared order, the
    literal trailing token `confirm` sets the confirmation flag — the
    **non-AI text leg** P1-12 requires;
  - a **machine-readable catalog** (JSON: ids, titles, params, capability,
    dangerous) — the discovery seed P1-13 will expose over the external API.
- A Qt `CommandController` (`src/workspace/`) that registers the existing
  native verbs as commands over the live controllers:
  `workspace.focus/priority/quality/layout/sweep/spatial`,
  `replay.start/replay.live` (honest refusal when no recording DB), and
  `device.rename` + **`device.remove` (dangerous → requires confirm)** when the
  device store is present.
- A **command palette** in the workspace (toolbar `⌘ Command` — the UI leg):
  type a command, see the deterministic result line and the session audit
  trail; the catalog lists available commands.
- `vms_workspace --commands` prints the JSON catalog; `--command-selftest`
  verifies the whole envelope headlessly.

## Excluded (own later gates)

- The **external control API** (transport, authentication, rate limits,
  streaming, agent connector) and the UI-coverage check — P1-13.
- **Voice** capture/parsing (the text grammar is the deterministic fallback it
  will feed) — P1-12's voice leg, later slice.
- Durable, immutable **audit storage** with trusted timestamps — P1-06.
- Real **RBAC**: P0-01A is Administrator-only, so the session grants all
  capabilities; the envelope checks membership so role separation later needs
  no rewrite (P1-02/P1-03).
- Rewiring every existing QML click through the envelope (the coverage gate is
  P1-13's acceptance); this pass proves the envelope and makes the palette a
  first-class invocation path.

## Acceptance criteria (headless)

1. Validation: unknown command, missing/extra param, bad type, out-of-range,
   and bad enum each yield the correct typed refusal and **do not execute**.
2. Capability: a command invoked without its capability is refused
   (`capability-denied`) and audited; granted → executes.
3. Dangerous: `device.remove`-class commands refuse without `confirm`
   (`needs-confirm`, nothing executed) and execute with it.
4. Text leg: parsed invocations drive the real `WorkspaceController` (e.g.
   `focus 5` moves the working set; `quality 3 thumb` caps the tier).
5. Every attempt — refused or executed — lands one audit record with outcome.
6. The JSON catalog lists every registered command with params/capability/
   dangerous so a machine can discover the surface.
7. Offscreen QML smoke loads the palette with no QML errors; all prior
   self-tests stay green.
