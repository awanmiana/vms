# Structured audit and trusted-anchor boundary - P1-06 second slice

> Native production scope approved by the owner's 2026-08-09 directive.
> This extends, and does not replace, `audit-log-P1-06-proposal.md`.

## Decision

Keep one append-only audit chain, version its canonical hash encoding, and add
structured redacted context needed by authentication, credential, access,
export, and configuration producers. Create an explicit RFC 3161 timestamp
boundary: the repository exports the exact SHA-256 chain head and persists a
timestamp token only after an injected cryptographic verifier accepts it and
the live chain still matches the request.

This does not call a public timestamp authority or label a local clock as
trusted. TSA selection, trust roots, policy OIDs, network policy, certificate
validation, revocation handling, and renewal are deployment/security choices.

## Included

- Schema v15, forward-only and additive.
- Canonical audit encoding v2 with category, actor, subject, correlation ID,
  and redacted before/after state. Migrated v1 rows retain their original hash
  semantics and continue to verify.
- An `audit_chain_anchor` receipt table carrying the anchored row range,
  SHA-256 head, request time, TSA URI/policy, base64 RFC 3161 DER token,
  verification time, and verifier identity.
- `makeAnchorRequest`, `recordVerifiedAnchor`, and `listAnchors` repository
  operations. Empty chains, incomplete evidence, non-SHA-256 algorithms,
  failed cryptographic verification, and a head changed after request all fail
  closed.
- Persistence and native audit self-tests covering migration/reopen, v2 field
  round-trip, unchanged legacy chain semantics, unverified-token rejection,
  verified receipt durability, and historical tamper detection.

## Deliberately not included

- A bundled or hard-coded TSA, private trust store, or network request.
- Retention/deletion. No row is pruned until a separately approved policy can
  prove an externally anchored prefix and retain a verifiable checkpoint.
- Operator login/MFA implementation, biometric processing, or export UI.
  Their future producers now have the structured storage contract they need.
- Raw secrets, credentials, biometric material, or unredacted arbitrary state.

## Standards references

- IETF RFC 3161, Time-Stamp Protocol: <https://www.rfc-editor.org/rfc/rfc3161.html>
- IETF RFC 5848, Signed Syslog Messages: <https://www.rfc-editor.org/rfc/rfc5848.html>
- NIST SP 800-92, Guide to Computer Security Log Management:
  <https://csrc.nist.gov/pubs/sp/800/92/final>

## Acceptance

1. Legacy audit rows still verify after schema v15 migration.
2. Every v2 field is covered by the row hash and round-trips without mutation.
3. An unverified token is never persisted.
4. A verified receipt is admitted only for the unchanged current chain head
   and survives reopen.
5. Existing persistence, command, workspace, and reference regressions pass.
