# External control API — P1-13 first slice (A0 mandate), native pass

> Scope note for native increment 26. Subordinate to `Development_plan.md`.
> Proceeding under the owner's standing "build the next thing, as you
> recommend" directive (2026-07-31, repeated). Builds directly on increment 25
> (the P1-12 command envelope).

## Problem & users

A0 requires the software to be drivable by an **external AI agent** (and any
programmatic client) through a **stable, authenticated, capability-scoped,
audited API** with the same authority model and confirmation gates as a human
operator (P1-13). Increment 25 built the single validated gate and its
machine-readable catalog; this slice puts a network transport in front of it.

## Included scope (this pass)

- A `CommandServer` (Qt HttpServer, `src/workspace/`) exposing the increment-25
  envelope over HTTP/JSON, **loopback-only** (binds `127.0.0.1`, never a public
  interface) and **off by default** (`--api-port N` enables it):
  - `GET /v1/commands` — the machine-readable catalog (the agent's discovery
    call, A0/P1-13);
  - `POST /v1/invoke` — `{"command": id, "args": {...}, "confirm": bool}` →
    the envelope's deterministic result as JSON. **Every** invocation flows
    through the same registry gate as the palette: validation, capability
    membership, dangerous-action confirmation, audit — the agent is a client
    of the envelope, never a privileged path.
  - **Bearer-token authentication** on every route (401 otherwise): a random
    per-session token generated at startup and printed once to the console
    (`--api-token <t>` overrides for scripted runs). The token grants the
    session's capability set (P0-01A: Administrator).
  - Deterministic HTTP status mapping: `ok`→200, `unknown-command`→404,
    `missing-param`/`bad-param`→400, `capability-denied`→403,
    `needs-confirm`→409, `failed`→500 — with the same JSON body shape
    (`{outcome, message}`) in every case.
- `--api-selftest`: spins the server on an ephemeral loopback port and drives
  it with a real HTTP client — no token → 401; catalog with token; a valid
  invoke moves the real workspace; typed refusals map to their status codes;
  the dangerous confirm gate holds over the wire; every attempt is audited.

## Excluded (own later gates)

- Multi-user auth, scoped tokens/roles, TLS, rate limits, and result/event
  **streaming** (rest of P1-13); remote (non-loopback) exposure is a
  security-reviewed decision of its own.
- The **UI-coverage check** (every click routes through the envelope) that
  closes P1-13, and the AI-agent connector itself (a client of this API).
- Durable audit storage (P1-06); the voice leg (P1-12).

## Data & security impact

Loopback-only, opt-in, bearer-authenticated; no secrets in URLs; invocations
carry the same capability checks + dangerous-action confirmation as the UI and
land in the same audit trail. No schema change.

## Acceptance criteria (headless)

1. Requests without (or with a wrong) bearer token get 401 and execute nothing.
2. `GET /v1/commands` returns the catalog an agent can drive the app from.
3. A valid `POST /v1/invoke` moves the **real** workspace and returns `ok`.
4. Refusals map deterministically (400/403/404/409/500) with honest messages,
   and a dangerous command without `confirm` executes nothing (409).
5. Every API attempt — refused or executed — lands in the same audit trail as
   the palette.
6. The server binds loopback only and is absent unless `--api-port` is given.
7. All prior self-tests stay green.
