# External control API rate limiting — P1-13 hardening slice

> Scope note for native increment 42. Subordinate to `Development_plan.md`.
> Approved by the operator's 2026-08-09 directive to enumerate the remaining
> work and continue building the next dependency-safe item.

## Problem

The loopback external-control API already requires a bearer token and routes
commands through the common validation, capability, confirmation, and audit
gate. It does not yet bound request volume. A faulty or hostile local client
can therefore consume the UI process's HTTP capacity without a deterministic
backpressure response.

## Included scope

- One server-wide fixed-window request budget covering catalog, invocation,
  unauthorized, malformed, and unknown-route requests.
- A secure default of 120 requests per 60 seconds whenever the API is enabled.
- `--api-rate-limit N` to lower or raise that deployment-local budget; zero and
  invalid values are refused rather than silently disabling protection.
- Deterministic HTTP `429` JSON responses with outcome `rate-limited`, the
  configured limit/window, and the remaining retry delay.
- Rate-limited invocations stop at the transport boundary and do not execute a
  command or change workspace state.
- End-to-end HTTP self-test coverage for admission through the configured
  budget and rejection immediately after it.

## Explicit exclusions

- Per-user/per-token policies (there is still one session token), distributed
  rate-limit coordination, adaptive abuse detection, or durable counters.
- TLS, remote/non-loopback exposure, scoped identities, roles, token rotation,
  streaming results/events, and an AI-agent connector. Those remain separate
  P1-02/P1-03/P1-13 security and product gates.

## Acceptance

1. The existing API behavior remains green below the default budget.
2. With a test budget of two requests per minute, requests one and two are
   served and request three returns structured HTTP 429.
3. A rejected invocation does not reach the command envelope and does not
   mutate the real workspace.
4. The production CLI reports the active budget at startup and refuses a
   non-positive configured budget.
