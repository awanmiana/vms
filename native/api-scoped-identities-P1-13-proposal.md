# Scoped external-control identities - P1-13/P1-06 slice

> Scope note for native increment 46. Subordinate to `Development_plan.md`.
> Approved by the operator's directive to complete every approved partial
> native capability using worldwide-standard behavior.

## Included

- Multiple bearer identities, each with a stable audit actor ID and an
  independent least-privilege capability set.
- Constant-shape token comparison and rejection of empty or duplicate
  identity/token definitions.
- Capability-filtered command discovery: an identity is not advertised verbs
  outside its grants.
- Invocation uses the identity's grants, not the Administrator UI session's
  grants. A denied request returns the existing structured HTTP 403 and cannot
  mutate the workspace.
- Caller-supplied safe `X-Correlation-ID` (or a generated UUID) is returned in
  results and persisted with the stable actor identity in structured v2 audit
  rows.
- Unknown routes now enforce bearer authentication as every other route does.
- Production flags `--api-identity` and `--api-capabilities` provide one
  scoped deployment identity; unknown/empty scopes fail closed.

## Security and deployment boundary

The service remains opt-in and bound only to `127.0.0.1`. It is suitable for
a same-host agent connector or an authenticated local reverse proxy. Remote
exposure remains disabled until the deployment hostname, certificate chain,
trust/revocation policy, and identity provider are supplied. This slice does
not make a false TLS or remote-security claim.

OAuth/OIDC identity federation, token issuance/rotation/revocation, direct TLS
termination, event/result streaming, and a packaged AI-agent client remain
P1-02/P1-03/P1-13 deployment slices.

## Verification

`--api-selftest` uses a full-capability and a zero-capability identity over
real loopback HTTP. It verifies filtered discovery, 403 with unchanged live
workspace, successful scoped invocation, identity/correlation result fields,
audit attribution, bearer rejection, deterministic errors, destructive
confirmation, and the existing fixed-window rate limit.

