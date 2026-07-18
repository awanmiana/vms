# VMS Operator Console

This is the first Windows-friendly prototype for the CCTV operator workflow. It focuses on camera identity, search, tags, area navigation, route-based previous/next cameras, live-view and remote-playback layout behavior, shared resource management, quick incident report preparation, and recorder device management.

## Approved product direction (P0-01)

The controlling [P0-01 architecture contract](Features%20Guide.md#p0-01-accepted-architecture-contract) defines the target product:

- A self-contained standalone installation for small deployments, with optional self-hosted coordination and scale-out components for shared sites and control rooms.
- No mandatory proprietary cloud, recurring vendor subscription, or forced telemetry.
- NVRs, DVRs, edge devices, or deployer-selected storage remain authoritative for continuous recordings. The VMS uses live streams transiently and requests playback/downloads on demand.
- Administrator-first development with stable capability boundaries so enforced role separation can be added later without redesigning every feature.
- No artificial commercial device limit. Actual capacity will be expressed as measured deployment profiles, with expensive work controlled through later device-activity priorities and media budgets.

The current runtime is only the **Standalone prototype**. Coordinated/scale-out services, real media bridges, production persistence, authentication, and measured capacity are later approval-gated work and must not be inferred from this README.

## Approved product boundary (P0-02)

The core is vendor-neutral. Reference Vendor is the first registered adapter identity and
the first planned device implementation, but no real Reference Vendor operation is
claimed yet. Adapter selection is explicit: a free-text vendor name or device type
cannot silently select Reference Vendor, open interoperability, or another integration.

Vendor/protocol code must remain behind the versioned adapter boundary. Missing or
unverified operations report `unsupported`, `unknown`, or `unavailable` instead of
being simulated or recorded as device failures. Other vendors can implement the
same contract without changes to core dispatch logic.

The project uses neutral sample data and does not claim affiliation with, endorsement
by, or compatibility with any device manufacturer. Dependency and asset licenses must
be reviewed before distribution.

## Approved failure behavior (P0-03)

The VMS is an optional data-acquisition, normalization, manipulation, presentation,
and interaction layer. Existing cameras, NVRs, and DVRs remain autonomous: stopping
or losing this software does not itself stop their native recording or operation.
Failure isolation applies inside the VMS so one failed site, device, adapter, stream,
or job does not unnecessarily freeze or falsify unrelated work.

The versioned policy layer under `shared/resilience/` keeps availability, operation
safety, multidimensional health, recovery, and resource-priority rules independent
from any particular database, worker, adapter, or UI implementation. Current
prototype behavior must remain honest when a dependency is unavailable: cached data
is marked stale/read-only, uncertain device state remains unknown, unsafe commands
are never queued for delayed replay, and failed persistence is not reported as a
successful durable save.

The complete contract is documented in
[`docs/failure-behavior.md`](docs/failure-behavior.md). It does not claim that a
production collector, coordinator, alarm/audit spool, authentication system, media
bridge, cluster, or VMS-owned recorder has been built. Those implementations remain
behind their later approval gates. Optional local or distributed VMS recording is
deliberately deferred until the core VMS workflow is mature.

## Run

Run the bundled local server so the browser UI and inventory API use the same origin:

```powershell
node backend\server.js
```

Then open:

```text
http://localhost:5173
```

Opening `index.html` directly is still supported as an offline mode. In that mode,
device, camera, and group inventory stays in the browser cache until the app is
opened through the bundled server again.

The current project has no root `package.json` and does not require an npm install.
The prototype server and regression tests use Node.js built-in modules.

## Why this shape

The first build is intentionally dependency-free so it runs immediately on Windows. The UI and data model are structured so a later stage can become a Windows desktop app using Electron/Tauri or a Qt shell while keeping the same operator concepts.

The browser shell should remain the executable interaction prototype and backend
test client through P0-04 and the initial media-contract work. Selecting a native
shell before data authority, persistence, codec, media-process, and capacity
requirements are approved would lock in deployment assumptions too early.

## Current modules

1. Monitor workspace with a Live View resource tree, camera/group search, fixed-grid and draggable spatial-canvas modes, live group controls, PTZ placeholders, digital zoom, fullscreen controls, and a selected-camera drawer.
2. Multiple persistent Live View workspace tabs. Each tab keeps its own open cameras, selection, grid division, and presentation state.
3. Remote Playback with a resource section matching Live View, independent camera/group search, fixed-grid and draggable spatial-canvas modes, date-range controls, a timeline/scrubber, and multiple persistent playback tabs.
4. Device Manager for generic prototype NVR, DVR, hybrid DVR, and direct IP-camera records; displayed vendor names are metadata, not compatibility claims.
5. Editable Camera Catalog for camera identity, tags, areas, recorder/channel mapping, and route metadata.
6. Click-based Route Builder for selecting a camera and mapping previous, next, and nearby cameras.
7. Incident report preparation and an Incident Center for saved live/playback reports, status tracking, and WhatsApp-ready text.
8. Site Map for placing cameras by area, position, and facing direction.
9. Camera Groups for saved focused layouts that open directly into Live View or Remote Playback, including auto-assigned device groups and custom cross-device groups.
10. A prototype User Access screen. P0-01 starts Administrator-only and permission-ready; the current role records do not provide production authentication or authorization, which remain later approval-gated work.
11. Settings with one shared fixed/draggable display switch, automatic/manual resource optimization, camera-session allocation feedback, media-policy visibility, last-grid restoration, area filters, and tag filters.
12. Collapsible left rail with Monitor, Playback, Events, Devices, Map, Reports, Access, and Settings navigation plus top camera search.
13. Light/dark theme support with a modern dark-neutral operator-console direction.

## Workspace and shared resource model

Live View and Remote Playback use one shared camera and group inventory while
keeping independent workspace state.

Each workspace tab owns:

- Its open camera placements.
- Its selected camera.
- Its grid division.
- Its Float/Dock and front/back presentation state.
- Its playback date range when it is a Remote Playback workspace.

The active Live View workspace is controlled by the Live View resource section.
The active Remote Playback workspace is controlled by the Playback resource
section. Opening, clearing, or changing one workspace does not overwrite another
workspace.

Workspace tabs are sticky and persistent. A workspace can be floated away from the
normal layout, dragged, resized, docked again, or moved in front of/behind the other
floating operational workspace.

Float is currently an internal MDI-style panel, not a separate operating-system
window. True external/native detached windows must wait for a central backend
resource lease manager; otherwise separate renderer processes could independently
exceed decoder, GPU, connection, or bandwidth limits.

One camera-session budget covers every Live View and Remote Playback tab,
including:

- Active and inactive tabs.
- Fixed grids and draggable canvases.
- Docked and floating workspaces.
- Front and back layers.

The same camera opened twice in one workspace counts once. The same camera opened
in two separate workspaces counts twice because each placement can require an
independent live or playback session.

Settings provides:

- **Automatic optimization:** a conservative prototype limit derived from
  browser-reported workstation CPU and memory information.
- **Manual optimization:** an operator-configured camera-session limit.

The global meter shows allocated sessions against the same effective limit in
Live View, Remote Playback, and Settings. Reducing the limit immediately releases
overflow placements in deterministic order and repairs affected selections.

This is a prototype admission governor, not a measured production capacity
profile. P0-05 through P0-13 must approve workstation, codec, hardware-acceleration,
deployment, and limit categories before the automatic values can be treated as
production guidance.

## Media status

Live and playback panes are channel-aware, but they do not decode real device video yet. Possible implementations below remain separately approval-gated:

- Live media through an approved adapter/protocol bridge.
- Recorder playback and download through an approved adapter.
- Standards/vendor profile import after protocol, device, firmware, licensing, and security evidence is approved.
- Production persistence selected under P0-04 instead of the temporary backend JSON store.

The target media model does not centrally ingest every camera around the clock. Recorders or deployer-selected storage retain continuous footage; approved live, playback, and download bridges will access it only when required.

The current stream-tier policy distinguishes `main`, `sub`, `thumb`, and `paused`
view work. That tier selection is separate from the later high/medium/low/idle
device-activity priority model.

Architecture planning files:

- `Features Guide.md` (controlling approval-gated roadmap, P0-01 deployment contract, P0-02 vendor boundary, P0-03 failure contract, and preserved market feature research)
- `docs/failure-behavior.md`
- `docs/UI_REDESIGN_PLAN.md`
- `docs/sqlite-schema.sql`
- `docs/media-policy.md`
- `docs/camera-tracking-architecture-spec.md`
- `docs/camera-tracking-architecture-spec-v2.md`
- `docs/compliance-query-architecture-spec.md`
- `docs/shared-data-layer-architecture-spec.md`
- `docs/sqlite-schema.sql` includes the target spatial-canvas tables, voice aliases, command log, breadcrumb spatial context, entity/location mapping, tag index, compliance history/logging, and WhatsApp query-ticket tables.

Backend foundation files:

- `backend/schema-loader.js`
- `backend/media-policy.js`
- `backend/repositories.js`
- `backend/commands.js`
- `backend/voice-regex.js`
- `backend/check.js`
- `backend/api-routes.js`
- `backend/server.js`
- `backend/device-adapters/` (vendor-neutral contract/registry plus the identity-only Reference Vendor shell)
- `backend/device-integration.js` (normalized integration-service boundary; no real vendor I/O yet)
- `shared/resilience/` (versioned P0-03 availability, command-safety, health, recovery, and resource-policy contracts)
- `backend/resilience/` (injectable backend facade over the shared pure policies; not a collector, queue, coordinator, or retry worker)

## Inventory persistence

When served by `backend/server.js`, the browser hydrates device, camera, and group
inventory from `GET /api/inventory` and sends debounced snapshots to
`PUT /api/inventory`. The browser cache remains an offline fallback. Passwords are
excluded from API snapshots and remain browser-local in this prototype.

The current backend uses the temporary `backend/vms-dev-db.json` atomic JSON-file
adapter. That file is not the approved production persistence architecture.

Every supported device type (`NVR`, `DVR`, `Hybrid DVR`, and `IP Camera Direct`)
gets one deterministic system group. Direct IP cameras are always normalized to
one channel. Camera ownership and group membership use stable device IDs, so a
device rename does not orphan or duplicate its cameras.

Workspace layout state is currently browser-local. Production shared layouts and
cross-client resource admission depend on the P0-04 authority decision and a later
backend lease/service boundary.

Run the complete dependency-free regression suite with:

```powershell
node test-all.js
```

Focused frontend checks include:

```powershell
node test-device-model.js
node test-workspaces.js
node backend\test-spatial-policy.js
```

## Native transition

Backend implementation and testing should continue now while this web shell acts
as the client and interaction harness.

Before true native or operating-system-detached workspaces, the backend needs:

1. P0-04-approved authoritative data ownership and persistence.
2. Stable APIs or IPC contracts for inventory, layouts, playback requests, and
   camera-session leases.
3. A central resource broker for decoder, GPU, connection, and bandwidth budgets.
4. Approved workstation and codec profiles under P0-05 through P0-13.
5. One verified real-media path that proves the process and failure boundaries.

After those decisions, Electron, Tauri, Qt, or another native stack can be
evaluated using measured requirements. The present frontend work remains useful
because it defines the operator behavior and can be hosted by a later desktop
shell if approved.

## Prototype gaps awaiting roadmap approval

The list below is historical planning context, not authorization to implement. Each item must follow the controlling roadmap's discussion and approval gate.

1. Discuss and approve P0-04, then replace the temporary JSON inventory store with the selected production persistence architecture.
2. Add secure device/NVR credential vaulting so operators never store NVR passwords directly.
3. Add open interoperability discovery and real channel/profile import.
4. Add RTSP preview using a native media bridge.
5. Add floorplan/background image import for the map canvas.
6. Add entity/location camera mapping plus compliance overlays on camera views.
7. Add strict `YYYY-MM-DD`, `HH:MM:SS`, and `YYYY-MM-DD HH:MM:SS` validation to operator-facing forms.
8. Add compliance log creation, including reviewed cameras, locations, footage time range, and manager response.
9. Add WhatsApp query-ticket ingestion and ticket-driven playback staging.
10. Connect playback panes to real NVR/DVR playback streams and evidence export.
11. Add WhatsApp Business/API handoff or one-click clipboard copy for report text.
12. Replace prototype user access with authenticated accounts and encrypted credential handling.
13. Move layout sharing and camera-session admission to an authoritative backend before enabling external detached windows or multiple native processes.
