# Durable audit history — P1-06 first slice, native pass

> Scope note for native increment 28. Subordinate to `Development_plan.md`.
> Proceeding under the owner's standing "build the next thing, as you
> recommend" directive (2026-07-31, repeated). Completes the durability gap
> increments 25–27 each named ("durable audit = P1-06").

## Problem & users

Every action already flows through one audited gate (the A0 envelope, inc 25):
palette lines, API calls, and alarm-panel clicks all produce audit records —
but only into a session trail and stdout. An audit history that disappears when
the app closes cannot answer "who did what, when, and was anything refused"
after the fact. P1-06 demands an immutable audit history; this slice delivers
its foundation: a **durable, append-only, tamper-evident** command-audit log.

## Included scope (this pass)

- **Schema v10**: an `audit_log` table (forward-only additive migration) —
  `time_utc, source, command, args, outcome, message, prev_hash, row_hash` —
  one row per envelope attempt, **refusals included**.
- An **`AuditRepo`** (`vms_persist`, Qt-free) whose contract is append-only:
  - `append(entry, hashFn)` — computes `row_hash = hashFn(prev_hash ‖
    canonical-fields)`, chaining every row to its predecessor (**tamper
    evidence**: editing or deleting any row breaks the chain from that point);
    the hash function is injected so the repo stays dependency-free (the app
    injects SHA-256);
  - `list(limit)` newest-first, `count()`, and `verifyChain(hashFn)` which
    walks the whole chain and reports the first broken row;
  - **no update or delete API exists** — immutability by contract at this
    layer (OS/file-level protection and trusted external timestamps are later
    P1-06 slices).
- The envelope's audit sink **also appends durably** when the store is open
  (same SQLite store, same transaction machinery), with an honest **source**
  attribution: `palette` (typed), `api` (HTTP), or `ui` (panel buttons and
  other in-app invocations).
- `--audit-dump [N]`: print the most recent N rows plus the chain-verification
  verdict (the operator/compliance read).
- `--audit-selftest`: rows persist across a reopen; refusals are recorded; the
  chain verifies; a simulated tamper (an UPDATE issued directly against the
  store) is **detected**; sources attributed; newest-first listing.

## Excluded (own later gates)

- The rest of P1-06's catalog: logins (needs P1-03 operator auth), credential
  use, export access, before/after configuration values, and **trusted
  (external) timestamps** — the local clock stamps rows in this slice.
- Retention/rotation policy for the audit table, and audit of non-envelope
  internals (closed by P1-13's coverage check as actions move into commands).
- Cryptographic signing/anchoring of the chain head (tamper *evidence* is in
  scope; tamper *proof* against an attacker who rewrites the whole chain and
  file is not claimable locally and is deferred with the coordinated gates).

## Acceptance criteria (headless)

1. Every envelope attempt (executed or refused, from palette/api/ui) lands one
   durable row; rows survive a store reopen.
2. `verifyChain` passes on an untouched log and reports the first broken row
   after a direct UPDATE tamper.
3. No update/delete surface exists on the repo; the schema migration is
   forward-only additive (v9 → v10 migrates cleanly, all prior suites green).
4. `--audit-dump` shows newest-first rows with source attribution and the
   chain verdict.
