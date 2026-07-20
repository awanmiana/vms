# Backend Foundation

This folder is the current **Standalone prototype** backend foundation. When the UI
is opened through `server.js`, device, camera, and group inventory is loaded from
and synchronized to this backend. Browser `localStorage` remains an offline cache,
and the current runtime uses an atomic JSON-file adapter.

The controlling [`Development_plan.md`](../Development_plan.md) owns all feature scope,
sequence, approval status, and verification evidence. This README describes only the
current backend and how to run it. Optional coordinated/scale-out services, production
persistence, synchronization, and high availability are not implemented here.

## Current contents

- `schema-loader.js`: reads the SQLite schema file and exposes statements for a future SQLite runtime.
- `media-policy.js`: current backend stream-tier and budget rules, pending C0-05 consolidation with the browser policy.
- `commands.js`: shared command contract and executor for UI clicks, gestures, regex voice, and later voice-agent commands.
- `voice-regex.js`: first STT transcript parser that turns simple spoken phrases into command objects.
- `repositories.js`: repository API shape for devices, cameras, users, sessions, and stream sessions.
- `file-db.js`: temporary development adapter with in-memory fallback.
- `services.js`: device onboarding, camera/channel reconciliation, automatic system groups, and media guard services.
- `device-adapters/`: versioned vendor-neutral adapter contract and exact registry. An identity-only reference shell is registered with no verified operations.
- `device-integration.js`: async normalized operation boundary requiring an explicit adapter ID. It never infers an adapter from free-text vendor/type metadata and never falls back to ONVIF.
- `../shared/resilience/`: versioned vendor-neutral availability, operation-safety, health, recovery, and resource-priority contracts shared independently of storage or transport.
- `resilience/`: injectable backend composition facade over those pure policies. It creates no timers, network calls, queues, databases, or background processes.
- `api-routes.js`: validated `GET/PUT /api/inventory` plus the existing integration stubs.
- `server.js`: dependency-free static server and inventory API host.
- `cli.js`: small local CLI for exercising the backend service shape.
- `test-*.js`: focused backend contract, service, integration, API, schema, media-policy, and resilience checks executed by the root suite.
- The exploratory target schema includes entity/location mapping, a generalized tag index, compliance event history, operator-confirmed compliance logs, and external-message query-ticket candidates.

## Sandbox note

In a restricted sandbox, Node may be blocked from writing `vms-dev-db.json`. When that happens, `file-db.js` falls back to in-memory operation so service checks can run. This behavior is not durable persistence; the production authority and adapter remain P0-04 decisions.

## CLI examples

```powershell
node backend\cli.js
node backend\cli.js devices:add-demo dev-demo "Demo NVR" 192.168.1.50 8
node backend\cli.js guard:plan 4
node test-all.js
```

## Pending persistence gate

The SQLite schema is an exploratory standalone option. Do not install or wire a
production database runtime until P0-04 decides data authority, persistence,
synchronization, backup, and restore behavior. Any approved persistence model must
store credential references rather than plaintext device passwords; the secret-store
technology is part of the security and platform decision.

## Failure-policy boundary

The normative P0-03 detail is
[`docs/failure-behavior.md`](../docs/failure-behavior.md). Current code implements
replaceable rules and honest prototype safeguards only—not a background continuity
provider, durable alarm/audit spool, authentication system, cross-site coordinator,
automatic retry scheduler, media download engine, access controller, or failover
cluster.

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
