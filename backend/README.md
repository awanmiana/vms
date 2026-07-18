# Backend Foundation

This folder is the current **Standalone prototype** backend foundation. When the UI
is opened through `server.js`, device, camera, and group inventory is loaded from
and synchronized to this backend. Browser `localStorage` remains an offline cache,
and the current runtime uses an atomic JSON-file adapter.

The controlling [P0-01 architecture contract](../Features%20Guide.md#p0-01-accepted-architecture-contract)
also permits optional self-hosted coordinated and scale-out deployments, but those
services are not implemented here. Persistence authority, database selection,
synchronization technology, and high availability remain later approval gates.
P0-03 now defines failure behavior through replaceable pure policies; it does not
turn this local server into a production coordinator, site agent, or durable event
service.

## Current contents

- `schema-loader.js`: reads the SQLite schema file and exposes statements for a future SQLite runtime.
- `media-policy.js`: shared stream tier and budget rules, including grid fallback and spatial-canvas zone tiers.
- `commands.js`: shared command contract and executor for UI clicks, gestures, regex voice, and later voice-agent commands.
- `voice-regex.js`: first STT transcript parser that turns simple spoken phrases into command objects.
- `repositories.js`: repository API shape for devices, cameras, users, sessions, and stream sessions.
- `file-db.js`: temporary development adapter with in-memory fallback.
- `services.js`: device onboarding, camera/channel reconciliation, automatic system groups, and media guard services.
- `device-adapters/`: versioned vendor-neutral adapter contract and exact registry. Reference Vendor is registered as an identity-only shell; it has no verified operations yet.
- `device-integration.js`: async normalized operation boundary requiring an explicit adapter ID. It never infers an adapter from free-text vendor/type metadata and never falls back to open interoperability.
- `../shared/resilience/`: versioned vendor-neutral availability, operation-safety, health, recovery, and resource-priority contracts shared independently of storage or transport.
- `resilience/`: injectable backend composition facade over those pure policies. It creates no timers, network calls, queues, databases, or background processes.
- `api-routes.js`: validated `GET/PUT /api/inventory` plus the existing integration stubs.
- `server.js`: dependency-free static server and inventory API host.
- `cli.js`: small local CLI for exercising the backend service shape.
- `test-commands.js`: assertion tests for command execution and regex voice parsing.
- `test-compliance-services.js`: assertion tests for tags, entity/location creation, compliance logs, tickets, and API stubs.
- `test-spatial-policy.js`: assertion tests proving frontend spatial tier logic still matches backend media policy.
- `test-resilience-*.js`: policy tests for autonomous source devices, cached read-only behavior, non-replayable physical commands, multidimensional health, distributed retry budgets, and graceful resource degradation.
- The target schema also plans entity/location mapping, a generalized tag index, compliance event history, operator-confirmed compliance logs, and WhatsApp query tickets for the operator workflow.

## Sandbox note

In the current Codex sandbox, Node may be blocked from writing `vms-dev-db.json`. When that happens, `file-db.js` falls back to in-memory operation so service checks still run. The real Windows app should write to SQLite in the application data directory.

## CLI examples

```powershell
node backend\cli.js
node backend\cli.js devices:add-demo dev-demo "Demo NVR" 192.168.1.50 8
node backend\cli.js guard:plan 4
node backend\check.js
node backend\test-commands.js
node backend\test-compliance-services.js
node backend\test-device-onboarding.js
node backend\test-device-adapter-contract.js
node backend\test-device-integration.js
node backend\test-api-server.js
node backend\test-schema-loader.js
node backend\test-spatial-policy.js
node test-device-model.js
```

## Pending persistence gate

The SQLite schema is an exploratory standalone option. Do not install or wire a
production database runtime until P0-04 decides data authority, persistence,
synchronization, backup, and restore behavior.

If SQLite is later approved for the standalone profile, candidate runtimes include:

- Node/Electron: `better-sqlite3`
- Tauri/Rust: `rusqlite`
- Qt/C++: Qt SQL SQLite driver

Do not store device passwords in SQLite. Store a `credential_ref` that points to Windows DPAPI / Credential Manager storage.

## Failure-policy boundary

The normative P0-03 contract is
[`docs/failure-behavior.md`](../docs/failure-behavior.md). Current code implements
replaceable rules and honest prototype safeguards only. It deliberately does not
implement a background continuity provider, durable alarm/audit spool, real
authentication, cross-site coordinator, automatic retry scheduler, media download
engine, access controller, or failover cluster.

Native camera/recorder operation is external to the VMS. A VMS outage can make
observations stale or unknown, but it is not evidence that native recording stopped.
Future VMS-owned local/distributed recording remains a separate later feature and
must plug in as an optional recording provider.

## Command layer

Every operator action should become one command object before it changes state. This keeps the app modular:

- Button click: emits `source: "ui_click"`.
- Map gesture: emits `source: "gesture"`.
- STT + regex voice: emits `source: "voice_regex"`.
- Future voice agent: emits `source: "voice_agent"`.

The executor does not care where the command came from. That is the important part for your voice-agent integration later.
