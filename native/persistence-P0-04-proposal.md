# P0-04 Authoritative Data and Persistence — scope proposal

> Status: **Draft for discussion** (2026-07-26). Nothing here is built. This is the
> discussion document the governance model requires before persistence product
> code. The controlling authority remains `../Development_plan.md` (P0-04 is
> `[ ] Discussed | [ ] Approved`). Mirrors the format of
> `governor-P3-03-proposal.md`.

## 1. The decision this gate actually makes

P0-04 selects the **persistence technology and the authority/ownership model** for
the delivered (native) runtime, and defines migrations, conflict resolution,
ordered duplicate-safe offline reconciliation, backup, restore, and export. The
plan is emphatic (`Development_plan.md` lines 176, 386, 85): **SQLite remains one
candidate, not a decision**, and no cleanup/adapter may pre-select it. This
proposal frames the decision and a conservative first increment; the owner makes
the call.

## 2. Intended users

- **Operators** — their workspace layout, selected cameras, groups, and settings
  survive a restart; a tracking session can resume.
- **Administrators** — provision devices, cameras, groups, and users durably.
- **Both deployment shapes** — standalone (local authority) and coordinated
  (shared server authority), per the N0 "same design, two deployments" decision.

## 3. Benefit

A single, durable **source of truth** the native client and the connection broker
read from and write to, with **honest failure** (P0-03C): a write that needs an
unavailable authority fails explicitly — never silently queued or reported saved.
Today the native `vms_workspace` holds all state in memory and loses it on exit;
there is no native persistence at all.

## 4. The decision space (this is the owner's call)

| Option | Shape | Fits | Cost |
| :- | :- | :- | :- |
| **A. Embedded local relational (e.g. SQLite)** | one local file, zero-install | standalone-first; matches N0 "user installs nothing" | no shared authority by itself |
| **B. Central database (e.g. Postgres)** | server-owned | coordinated deployment, RBAC + audit | requires a server; premature for standalone |
| **C. Cloud service** | managed, networked | multi-site, hosted | latency/offline; external dependency |
| **D. Synchronized edge/server ownership** | edge cache + server authority + sync | large multi-node | conflict resolution, the hardest path |

**Recommendation (for discussion, not a decision):** adopt **A** — an embedded
local relational store — as the standalone local authority **behind the
technology-neutral C0-03 adapter contract**, and **defer B/D** (shared authority,
sync, offline conflict reconciliation) to the coordinated gate (P1-10). This
matches the N0 ARCHITECTURE record and the broker's "same interface, two
deployments" intent: the coordinated server can later implement the same adapter
contract against a central DB without changing the client. If you prefer to decide
the coordinated authority model now, that enlarges this gate substantially.

## 5. Included scope (first pass, if the recommendation is approved)

- A **native persistence layer** implementing the **C0-03 adapter contract**
  (atomic replace, rollback, declared-table schema, typed persistence errors) over
  the chosen embedded store. Consumers never reach into store internals.
- A **versioned forward-migration** mechanism (`schema_migrations`), idempotent.
- Only the **entities the native client needs now**: `devices`, `cameras`,
  `device_credentials` (a `credential_ref` only — the actual secret is the next
  gate, DPAPI), `camera_groups` + members, `operator_settings`, `stream_profiles`,
  and **workspace layout state** (tile count, per-camera desired tier + priority,
  active tab). Not the full 37-table schema.
- **Backup / restore / export** (integrity-checked file copy for the embedded
  store; a portable, re-importable snapshot).
- **Honest failure semantics** (P0-03C) wired to the adapter's typed errors.

## 6. Explicitly deferred (out of this pass)

- Central DB, cloud, multi-node **synchronization**, and **offline conflict
  resolution / ordered duplicate-safe reconciliation** → coordinated gate (P1-10).
- The remaining reference tables (users/permissions/sessions, incidents, tickets,
  tracking breadcrumbs, spatial canvases, compliance, PTZ/command logs) until a
  feature actually needs them.
- **Credential secrets** (DPAPI encrypt/decrypt) → the next approved gate
  (connection broker + credential store). This gate stores only the `credential_ref`.

## 7. Reference alignment

The behavioral spec is the reference backend — `backend/device-adapters/contract.js`
(the adapter contract), `backend/file-db.js` + `backend/repositories.js` +
`backend/schema-loader.js`, and the drafted `docs/sqlite-schema.sql` (37 tables).
The native store mirrors the **adapter contract and table semantics**, not the JSON
file format. C0-03 already moved atomic replace / rollback / declared schema /
typed errors behind that boundary, technology-neutrally — this gate provides the
first production implementation of it.

## 8. Measurable acceptance criteria

1. **Durability:** devices, cameras, groups, settings, and the workspace layout
   persist across a full restart (verified via a native persistence test tool or
   `vms_workspace` save/restore).
2. **Migrations:** schema migrations apply forward, versioned, and idempotently
   (re-running a migrated DB is a no-op).
3. **Atomicity:** a crash/kill mid-write leaves the last consistent state — no
   partial or corrupt record.
4. **Backup/restore:** backup → wipe → restore round-trips byte-identical state.
5. **Export/import:** export produces a portable snapshot that re-imports cleanly.
6. **Honest failure:** a write requiring an unavailable authority fails with a
   typed error; nothing is silently queued or reported saved (P0-03C).
7. **Boundary:** all access goes through the C0-03 adapter contract; no consumer
   reaches store internals.

## 9. Alternatives considered

- **Keep the JSON file-db** (current prototype) — rejected as production authority:
  no transactional guarantees, no migrations, not the delivered runtime.
- **Decide the central/synced authority now** — rejected for a standalone-first
  first pass: it front-loads the hardest problem (multi-node conflict resolution)
  before there is a coordinated deployment to serve.

## 10. Proposed build sequence (native increment 5, each verified on hardware)

- **5a — store + contract spike:** the embedded store + the C0-03 adapter contract
  + the migration runner, with a `vms_dbtest` CTest self-check (the character of
  increments 1–3 and `vms_govtest`).
- **5b — entities + durability:** the entities in §5 + backup/restore/export +
  atomicity test.
- **5c — wire the workspace:** `vms_workspace` persists and restores its layout,
  camera list, and per-camera tier/priority across restart.

## 11. Recorded Approval Log scope (draft — for the owner to approve/edit)

> P0-04 | <date> | Approved (scope) | First persistence pass: an embedded local
> relational store as the **standalone** local authority behind the C0-03 adapter
> contract; versioned forward migrations; the entities the native client needs now
> (devices, cameras, credential_ref, groups, operator settings, stream profiles,
> workspace layout); backup/restore/export; honest failure per P0-03C. Central DB,
> cloud, synchronization, offline conflict reconciliation, credential secrets
> (DPAPI), and the remaining reference tables are deferred. | Acceptance criteria
> §8 | Selects the standalone local store only; the coordinated authority/sync
> model remains P1-10.
