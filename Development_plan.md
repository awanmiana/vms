# VMS Development Plan

> **Status:** This is the repository's single controlling development plan, complete feature catalog, development sequence, approval ledger, implementation-status record, and verification checklist. README files describe how the current prototype works; retained files under `docs/` are approved or implemented technical contracts and do not maintain a second backlog or build order.
>
> **Reference coverage:** A structural and item-by-item audit against the supplied original 623-line vendor-research guide on 2026-07-19 confirmed that all 143 original P0-P12 roadmap items and their statuses are represented here. The copied research appendix was excluded after its capability families were mapped to the roadmap.

## Document Ownership and Third-Party Naming

- This file alone answers **what is approved, what is next, what is built, and what has been verified**.
- A technical specification may define how an approved item should behave, but it cannot change priority or status. If a specification conflicts with this file, this file controls.
- Existing prototype code is recorded separately from roadmap status. Code that predates approval does not make its corresponding feature `Approved`, `Built`, or `Verified`.
- The external reference guide is discovery material, not a second roadmap and not evidence that a third-party capability is supported.
- The public roadmap uses capability-based wording. Exact third-party product or service names belong only in dated evidence records, adapter manifests, compatibility matrices, procurement records, or integration specifications where identification is necessary.
- Standards and protocols such as ONVIF, RTSP, H.264, H.265, and SQLite may be named when they identify the exact technical contract under evaluation. Naming a standard or third party does not imply affiliation, endorsement, certification, or compatibility.
- Vendor prose, product catalogs, part numbers, commercial terms, regional claims, and hardware figures must not be copied into this roadmap. Any such claim must be independently verified under P0-14 and P12-11 before it affects product scope.

## Approval Contract

1. We will work through this checklist in dependency order, one approval item at a time unless an explicitly approved dependency requires a different order.
2. Before any product code is written for an item, Codex must discuss its problem, intended users, benefit, included and excluded scope, workflow, dependencies, data and security impact, measurable acceptance criteria, and important alternatives with the project owner.
3. The project owner must explicitly approve the agreed scope. Silence, an existing vendor feature, an existing implementation, or approval of another item does not count as approval.
4. Product code may start only when both the **Discussed** and **Approved** boxes for that item are checked and its decision is recorded in the Approval Log.
5. **Built** is checked only after implementation is complete. **Verified** is checked only after the agreed tests and acceptance criteria pass.
6. An item may instead be recorded as **Changed**, **Deferred**, **Rejected**, or **Out of scope**. Deferred or rejected work must not be implemented.
7. Benefits, capacity limits, commercial rules, regional availability, and safety/privacy claims are gates or acceptance criteria. They are not silently converted into product features.
8. Any capability depending on a vendor-specific product or managed service requires confirmation of the exact product, mode, version, device/firmware, API or SDK, license, tenant, target country, and support terms before approval.
9. Existing project behavior will be reviewed against the relevant item; it will not be assumed correct merely because it already exists.

### Status Legend

- **Discussed:** requirements and tradeoffs were reviewed with the project owner.
- **Approved:** the project owner explicitly authorized the recorded scope.
- **Built:** the approved scope was implemented.
- **Verified:** acceptance tests and evidence passed.
- **Type labels:** Feature, Integration, Architecture, NFR, Policy, Commercial, or Benefit.

Each roadmap line uses this order:

`[ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified`

For a Policy, Commercial, or Benefit item, **Built** means the agreed policy, configuration, evidence, or measurable acceptance target was put in place; it does not necessarily mean application code was added.

## Approval Log

| Item | Discussion date | Decision | Agreed scope | Acceptance criteria / evidence | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| P0-01 | 2026-07-15 | Approved, built, and verified | Open-source hybrid VMS; permission-ready Administrator-first design; recorder-owned footage; standalone plus optional self-hosted coordination/scale-out; large distributed deployments; priority-based device activity | Accepted Architecture Contract, documentation consistency audit, Administrator-only prototype migration, and full regression suite | P0-01 is complete; later gates remain unapproved |
| P0-02 | 2026-07-15 | Approved, built, and verified | Independent vendor-neutral VMS core with one identity-only reference adapter | Versioned async adapter contract, exact registry, identity-only reference shell, explicit port validation, arbitrary-adapter/no-fallback tests, documentation audit, and full regression suite | Real vendor/protocol operations remain unapproved |
| P0-03 | 2026-07-17 | Approved, built, and verified | Standards-compatible modular failure behavior for an optional VMS data/interaction layer; autonomous source devices and recorder-owned footage; honest degraded/read-only/unknown states; non-replayable physical commands; replaceable resilience policies | Normative failure-behavior contract, standards-correction audit, shared versioned policy modules, injectable backend facade, current-prototype honesty fixes, focused policy/integration tests, browser verification, and full regression suite | Concrete collectors, queues, databases, authentication, media bridges, access controllers, clusters, and VMS-owned recording remain later gates |
| C0 | 2026-07-19 | Build order approved; documentation consolidation built and verified; code consolidation pending | Build each shared domain rule, service primitive, and operational UI component once before adding another screen | C0 checklist below, duplicate-authority audit, reference-coverage audit, document-role cleanup, and full regression suite | C0 changes architecture, not unapproved product scope or the P0-04 persistence decision |

## Immediate Track C0 - Build Once, Use Everywhere

> The owner approved this consolidation order on 2026-07-19. C0 removes duplicate implementations and contradictory contracts before feature delivery resumes. It may refactor current adapters and prototype UI, but it must not select production persistence, add a real media bridge, or implement another unapproved product feature.

- [x] Discussed | [x] Approved | [x] Built | [x] Verified - **C0-01 [Architecture] One development authority.** Keep every feature, delivery sequence, approval status, carryover item, and verification check in this file. README files remain operational descriptions; technical documents remain reference specifications without their own build orders.
- [x] Discussed | [x] Approved | [ ] Built | [ ] Verified - **C0-02 [Architecture] Canonical inventory contracts and reconciliation.** Create one versioned device/camera/group model, one legacy upgrader, one stable ID policy, one device-channel reconciliation function, and one system-device-group builder. UI, API, services, JSON storage, and future SQL mappings must consume those contracts rather than reinterpret them.
- [x] Discussed | [x] Approved | [ ] Built | [ ] Verified - **C0-03 [Architecture] Transactional current persistence boundary.** Move atomic replace, rollback, declared-table schema, and typed persistence errors behind the current store adapter. Routes must not reach into adapter internals. This refactor must remain technology-neutral and must not pre-approve SQLite or any central database under P0-04.
- [x] Discussed | [x] Approved | [ ] Built | [ ] Verified - **C0-04 [Architecture] One operation and resilience vocabulary.** Centralize operation IDs; explicitly map adapter, policy, command, persistence, and HTTP outcomes; convert legacy health observations at their boundary; and make command/device execution consume the shared resilience policy service.
- [x] Discussed | [x] Approved | [ ] Built | [ ] Verified - **C0-05 [Architecture] Shared policy and utility primitives.** Use one browser/Node media policy, one stream-profile source, and shared configuration, validation, time, identifier, error, stable-sort, and policy-registry primitives where semantics are truly identical.
- [x] Discussed | [x] Approved | [ ] Built | [ ] Verified - **C0-06 [Architecture] Repository and composition boundary.** Define repository interfaces for the canonical domain, implement the current file-store adapter, inject repositories/services at one composition root, and remove parallel service construction.
- [x] Discussed | [x] Approved | [ ] Built | [ ] Verified - **C0-07 [NFR] Shared test infrastructure.** Move to one test runner style with reusable memory database, HTTP server, app loader, and domain factories; keep `node test-all.js` as the complete compatibility entry point.
- [x] Discussed | [x] Approved | [ ] Built | [ ] Verified - **C0-08 [Architecture/Feature] One operational workspace component family.** Replace separate Live and Playback trees, rows, grids, toolbar handlers, popovers, fullscreen logic, workspace defaults, and legacy CSS overrides with two independently stateful instances of one configurable component family. Playback-only controls remain mode extensions.

### C0 Verification Checklist

- [x] The external reference and this guide contain the same 143 original P0-P12 items and the same pre-consolidation status.
- [x] The copied research prose, product catalog, part numbers, vendor-derived hardware/capacity tables, commercial claims, and regional claims are not embedded in the controlling roadmap.
- [x] Every repository document declares a single role: controlling roadmap, current-use README, normative approved contract, or non-authorizing reference specification.
- [x] No repository document outside this file maintains an independent build order, backlog, Definition of Done, or feature-status checklist.
- [ ] One inventory contract version and upgrader are consumed across browser, API, service, storage, and tests.
- [ ] Device channel reduction, detachment, IDs, aliases, and automatic groups have one tested result across every layer.
- [ ] Persistence replacement and rollback are adapter-owned, declared tables reject drift, and routes do not access store internals.
- [ ] Operation identifiers are registered once and every cross-domain outcome conversion is explicit and tested.
- [ ] Media tiers, bitrates, capacity guards, timestamps, IDs, validation, and coded errors each have one appropriate authority.
- [ ] Services depend on repositories and one composition root; no production path uses a throw-only or parallel authority.
- [ ] Test helpers replace repeated harnesses and fixtures while `node test-all.js` remains green.
- [ ] Live and Playback render through the same operational component/controller family with mode-specific slots and independent state.
- [ ] Browser-level resize, stacking, floating, fullscreen, accessibility, listener-lifecycle, and Playback timeline/segment tests pass.

## Current Prototype Checkpoint - 2026-07-19

This checkpoint records code that exists for evaluation. It does **not** check the P2, P3, or P5 roadmap boxes, because the corresponding production scope, real integrations, and full acceptance criteria remain unapproved.

### Shared Operational UI Scaffold

- [x] The camera stage/grid is the full background surface in Monitor and Playback.
- [x] Workspace tabs, camera/group browser, per-workspace search, selected-camera inspector, and shared toolbars layer over the grid; the primary application rail remains outside the workspace stack.
- [x] Playback keeps start/end date-time inputs, transport, day navigation, speed control, a requested-range footage bar, and a draggable playhead as a bottom mode extension.
- [x] Fixed grids and spatial canvases follow their containers in docked, floating, restored, and narrow-window source-level regression checks.
- [x] Live and Playback can be independently floated and ordered front/back without allowing either workspace to cover the primary rail.
- [ ] Common UI is rendered by one component/controller family rather than parallel Live and Playback implementations.
- [ ] A real-browser viewport matrix verifies resize, page-scroll, stacking, fullscreen, float recovery, and hidden-to-visible canvas behavior.
- [ ] Monitor snapshot and recent-event behavior is functional and capability-gated.
- [ ] Playback receives fresh recorder-backed availability segments and distinguishes available, missing, unknown, overlapping, and multi-source footage.
- [ ] Real live/playback media, evidence export, secure credentials, authenticated access, and authoritative production persistence pass their controlling gates.

### Approved Operational Workspace UI Decision

The following bounded UI direction is approved for the shared scaffold and C0-08. It does not approve real media, evidence, device, or persistence behavior:

1. Mount one Live instance and one Playback instance of the same configurable workspace component family. Do not move one literal DOM panel between routes; both workspaces may be visible or floating at once.
2. Within each workspace, layer back-to-front: camera stage/grid; Playback timeline/range extension when applicable; resource browser and selected-camera inspector; shared grid toolbar and workspace tabs; per-workspace search; popovers, menus, and tooltips.
3. The camera stage always fills the usable workspace. Opening an overlay must not reflow or shrink the grid. Overlay panels scroll internally; operational pages do not scroll.
4. Live mode supplies digital zoom, PTZ, snapshot, stream settings, and recent-event extensions. Playback supplies start/end date-time selection, transport, speed/day navigation, availability segments, draggable playhead, and evidence actions.
5. Each instance owns independent query, selection, panel state, grid division, maximized camera, tabs, canvas runtime, and playback range. Shared inventory and resource admission remain common services.
6. Sizing is container-driven with recoverable dock/float/fullscreen transitions. Narrow layouts overlay controls rather than forcing a fixed-width canvas beyond the viewport.
7. Requested time is never labeled as recorded or available footage. Availability, gaps, and source identity require fresh evidence from the selected recorder or recording service.

### Operational Workspace Acceptance Checks

- [ ] At representative desktop and restored-window sizes, document scroll remains absent, stage bounds equal workspace bounds, overlays remain inside the workspace, and the application rail never intersects a workspace.
- [ ] Computed stacking order is stage < Playback timeline < panels < toolbar/tabs/search < popovers, with the active floating workspace above the other.
- [ ] Fixed divisions and both spatial canvases exactly track container size after resize, rail collapse, dock/float, fullscreen, maximize/restore, and hidden-to-visible transitions.
- [ ] Opening the resource browser or inspector does not change stage bounds; both panels remain keyboard reachable and internally scrollable.
- [ ] Live query, selection, panels, grid, and maximize state never mutate Playback state, and the reverse is also true while both are visible.
- [ ] Shared camera-session limits include all active/inactive, docked/floating, and front/back workspaces without double-counting one camera inside one workspace.
- [ ] Playback validates start before end, handles invalid/empty/time-zone cases, supports pointer and keyboard scrubbing, and exposes accessible playhead value text.
- [ ] Availability tests cover continuous/event segments, gaps, overlaps, duplicate sources, source outage, partial-camera results, and unknown state.
- [ ] One handler is registered per action, observers/listeners are cleaned up, inactive work is bounded, and lifecycle tests detect duplicate dispatch.
- [ ] Keyboard focus, screen-reader labels, non-color status cues, reduced motion, loading, empty, unsupported, stale, read-only, and partial-error states are verified.

## Next Decision and Delivery Lanes

There is one controlling order with parallel work only where it does not pre-empt an unapproved product decision:

1. **Next code task:** C0-02 canonical inventory contracts and reconciliation, followed by C0-03 through C0-08 in their listed order.
2. **Next product decision:** P0-04 authoritative data and persistence. C0 may clean the current adapter but may not select the production technology or authority model.
3. **Frontend prototype lane:** after C0-08 and a separately recorded prototype authorization, the historical screen sequence is Devices, Events/Incidents, Reports/Compliance/Tickets, then Settings/Map/Access polish. A prototype screen does not check its P-phase feature boxes.

| Frontend slice | Current state | Owning roadmap items |
| :--- | :--- | :--- |
| Application shell | Prototype scaffold present | P3-14 / C0-08 |
| Monitor | Layered scaffold present; snapshot/recent events and real media remain incomplete | P3-01 through P3-17 |
| Playback | Layered scaffold and requested-range timeline present; real availability/media/evidence remain incomplete | P5-01 through P5-10 |
| Devices | Existing prototype inventory screen; redesign and real onboarding/integration remain unapproved | P2-01 through P2-14 |
| Events/Incidents | Existing prototype elements; production event/alarm lifecycle remains unapproved | P5-08/P5-09 / P6-01 through P6-13 |
| Reports/Compliance/Tickets | Existing report/compliance stubs and services; dedicated workflow remains unapproved | P1-11/P1-12 / P5-10 / P6-11 through P6-13 |
| Settings/Map/Access | Prototype surfaces only; production capability, map, identity, and access scope remain unapproved | P0-05 through P0-17 / P1 / P7 / P8 |

## Phase 0 - Product Direction, Evidence, and Capacity Gates

- [x] Discussed | [x] Approved | [x] Built | [x] Verified - **P0-01 [Architecture/Benefit] Product and deployment goal.** The project owner explicitly approved the complete P0-01 direction on 2026-07-15. The accepted contract, documentation alignment, and Administrator-only permission-ready prototype baseline are implemented and verified. This completion does not authorize implementation belonging to later roadmap gates.

### P0-01 Approved Decision Record

> These entries are approved as the product direction. They authorize the P0-01 architecture contract only. Detailed feature and infrastructure implementation remains subject to its own later roadmap gate.

| Sub-item | Topic | Current state | Approved decision |
| :--- | :--- | :--- | :--- |
| P0-01A | First users | Accepted | Release one starts with one **Administrator** role having all available privileges. Features must be designed around stable capabilities/permission boundaries so later role separation does not require rewriting them. The role editor and feature-to-role assignment will be built after the initial feature set; strong role-based permissions remain a later requirement under P1-02/P1-03. |
| P0-01B | Site pattern and scale | Accepted | The intended architecture supports many local and remote control rooms, sites, and operators, with an eventual scale direction of hundreds of thousands of registered cameras and thousands of NVRs/DVRs. The software must not impose artificial commercial limits. Measurable release profiles and horizontal-scaling boundaries will be defined and tested later; "any number" is not an untested capacity guarantee. |
| P0-01C | Primary topology | Accepted | **Hybrid** means self-contained standalone local operation plus optional self-hosted coordinated and scale-out operation for shared multi-site/control-room use. It does not imply a required proprietary cloud subscription. Exact service placement, synchronization, and failure behavior remain later gates. |
| P0-01D | Cost and licensing | Accepted | The product is intended to be open source and must not require a premium vendor plan, mandatory proprietary cloud, recurring vendor subscription, or forced telemetry. The source research is a feature-discovery input, not a dependency or licensing plan. The repository's specific open-source license remains a later owner decision. |
| P0-01E | Video-recording ownership | Accepted | The VMS will not normally ingest every camera continuously into its own central archive. NVRs, DVRs, devices, or user-selected storage remain authoritative for recordings. The VMS opens live streams when needed and requests recorded footage for playback/review or download on demand through supported device links/protocols. Downloaded evidence scope is handled separately under P5-08/P5-09. |
| P0-01F | Enterprise architecture | Accepted at product-direction level | Optional self-hosted coordination stores shared VMS metadata, not continuous footage. Workers/site agents, load balancing, background processing, capacity-aware optimization, device activity priorities, and an optimization report may be added as scale requires. Exact database, service, high-availability, backup, auditing, and monitoring designs remain later gates. |
| P0-01G | Data control | Accepted | The deployer controls where VMS metadata, logs, settings, credentials, and downloaded evidence are stored. The core has no forced cloud, telemetry, or country-specific hosting assumption. Country-specific legal deployment remains the deployer's responsibility. |
| P0-01H | Latency | Accepted; measurements deferred | Build functional device-management and media paths first using verifiable VMS-like connection/stream-selection/reconnect behavior. Establish numeric defaults through protocol/device research and tests against working software, then optimize iteratively and validate before production readiness. |
| P0-01I | Distributed-site operations | Accepted as roadmap scope | The roadmap includes centralized site/device visibility, site-scoped permissions, remote health monitoring, cross-site live view/playback, time-zone handling, WAN/cloud outage behavior, central alarm escalation, and per-site retention/security configuration. Each capability still requires its own later approval gate. |
| P0-01J | Local infrastructure | Accepted | Small installations require minimal mandatory infrastructure. Larger installations add optional self-hosted coordination, databases, workers, and site agents as needed; they do not depend on a paid vendor cloud. Cameras, recorders, networks, storage, and operator machines remain deployment requirements. |
| P0-01K | Release-one priority order | Accepted | Prioritize scalable device orchestration, device manipulation, and compliance workflows without centrally recording every 24/7 stream. Build working behavior before latency tuning. Use activity/priority states so only the necessary subset of devices maintains expensive streaming or high-frequency work. Exact conflict rules and resource budgets remain later design/test gates. |
| P0-01L | Device activity and priority model | Accepted as a concept | Devices/connections will have resource states such as **high**, **medium**, **low**, and **idle**. High-priority devices may stream or receive frequent processing; idle devices must not consume continuous streaming resources. State meanings, automatic/manual transitions, alarms that can elevate priority, fairness, and per-host limits will be discussed before this feature is implemented. This is separate from per-view media-quality tiers such as main/sub/thumb/paused. |

### P0-01 Accepted Architecture Contract

The following statements are normative for future development:

- The VMS **must** work as a self-contained standalone installation for small deployments.
- The VMS **must** be able to grow through optional, self-hosted coordination, shared metadata services, workers, and site agents; no managed vendor cloud is mandatory.
- Continuous recordings **must** remain owned by NVRs, DVRs, edge devices, or storage selected by the deployer unless a later, separately approved recording feature explicitly changes that behavior.
- The VMS **must** use live streams transiently and request recorder playback/downloads on demand; it **must not** create an implicit central 24/7 archive.
- The initial application role is Administrator, but feature boundaries **must** remain permission-ready for later RBAC.
- The product **must not** impose artificial commercial limits. Published limits must be measurable technical/deployment profiles rather than unlimited claims.
- The deployer **must** control VMS data placement. The core **must not** require forced telemetry, a proprietary cloud, or a vendor subscription.
- Device activity priority and media quality are separate controls. High/medium/low/idle manages connection/work cost; main/sub/thumb/paused manages a selected view's media quality.

#### Deployment Profiles

| Profile | Required components | Intended use | P0-01 boundary |
| :--- | :--- | :--- | :--- |
| Standalone | Operator application plus local backend/data adapter | Small or isolated installation | Current prototype direction; exact production persistence awaits P0-04 |
| Coordinated | Multiple clients plus an optional self-hosted coordination/API and shared metadata layer | Shared sites, control rooms, operators, alarms, jobs, and audit | Product contract only; service/auth/sync details are not yet approved |
| Scale-out | Coordinated profile plus optional workers, site agents, and distributed processing | Very large registered inventories and concurrent workloads | Architecture direction only; tested limits and partitioning remain later gates |

#### Data and Media Ownership

| Data/media | Authoritative owner or location | P0-01 rule |
| :--- | :--- | :--- |
| Continuous recordings | NVR/DVR/edge device or deployer-selected storage | Not copied into a mandatory VMS archive |
| Live media | Source device while an approved view/operation needs it | Transient and resource-budgeted |
| Playback media | Recorder/device/user-selected recording service | Requested on demand |
| Downloaded clips/evidence | Location explicitly selected or configured by the deployer | Separate evidence/retention gates apply |
| VMS inventory/configuration/health/alarm/job/audit metadata | Standalone local store or optional self-hosted shared store | Exact authority, synchronization, backup, and HA await later gates |
| Credentials/secrets | Deployer-controlled secure storage | Exact vaulting and access rules await later security gates |

#### Deliberately Deferred Decisions

| Deferred detail | Owning roadmap gate |
| :--- | :--- |
| Product/vendor integration boundary and open-source license selection | P0-02 / P0-17 |
| Outage behavior and high availability | P0-03 / P12-04 |
| Database choice, authority, synchronization, backup, and restore | P0-04 / P1-10 |
| Hardware/codec profiles, numeric capacity, and optimization thresholds | P0-05 through P0-13 / P12-03 |
| Security, privacy, authentication, enforced roles, and audit implementation | P0-16 / P1-02 / P1-03 / P1-06 |
| APIs, queues, background workers, load balancing, and site-agent protocols | P1-05 / P7-03 |
| High/medium/low/idle transitions, budgets, fairness, and latency | P2-13 / P3-03 / P6-08 / P12-03 |
| Recorder playback/download and evidence behavior | P4-03 / P5-01 / P5-02 / P5-08 / P5-09 |

#### Current-Project Consistency Evidence

- The current loopback backend and browser-local fallback are a temporary **Standalone** prototype, not the coordinated/scale-out implementation.
- Existing main/sub/thumb/paused media policy and per-recorder stream guards align with resource-conscious viewing, but do not implement the approved high/medium/low/idle device-activity model.
- Current JSON/localStorage full-snapshot persistence is not a scalable target and must not be presented as one.
- Real recorder live/playback/download bridges, centralized coordination, authentication/RBAC enforcement, capacity reporting, and scale-out workers are not implemented by P0-01 and remain gated work.
- No mandatory vendor cloud or telemetry dependency exists in the current runtime.

#### P0-01 Verification Checklist - 2026-07-15

- [x] The controlling roadmap records explicit owner approval and all P0-01A through P0-01L decisions as accepted.
- [x] Standalone, optional self-hosted coordinated, and scale-out profiles are defined without claiming that unbuilt services already exist.
- [x] Continuous recorder-owned footage, transient live media, on-demand playback/download, VMS metadata, evidence, and credentials have distinct ownership boundaries.
- [x] Administrator is the only prototype role; legacy prototype role records normalize to Administrator with the complete current capability registry.
- [x] Stable capability identifiers are retained so later RBAC can assign features without redesigning their boundaries.
- [x] Device activity priorities are documented separately from existing main/sub/thumb/paused per-view media tiers.
- [x] Project documentation consistently states that JSON/localStorage is a temporary standalone adapter and that database/coordinator choices remain later gates.
- [x] No mandatory vendor cloud, subscription, or telemetry dependency was introduced.
- [x] JavaScript syntax checks and the complete dependency-free VMS regression suite pass.
- [x] Product code for unapproved database, coordinator, HA, RBAC enforcement, device-priority scheduling, capacity optimization, or media bridges was not added.

- [x] Discussed | [x] Approved | [x] Built | [x] Verified - **P0-02 [Architecture] Product boundary.** The project owner approved an independent, vendor-neutral core with one identity-only reference adapter. The versioned boundary, exact registry, identity-only shell, normalized outcome safeguards, neutral port handling, documentation, and tests are implemented and verified. Real vendor-specific, ONVIF, RTSP, SDK, and cloud operations remain unapproved until their individual evidence gates pass.

### P0-02 Accepted Vendor-Boundary Contract

- The product is an independent, vendor-neutral VMS. Its first registered integration is an identity-only reference adapter, not the product identity, an exclusive dependency, or a claim of current device compatibility.
- The core owns normalized sites, devices, cameras/channels, groups, users/capability identifiers, jobs, events, health records, playback requests, evidence records, errors, UI workflows, persistence contracts, media policy, and auditing concepts.
- Optional adapters own vendor/protocol identity, authentication/session details, device/model/firmware interpretation, discovery, channel/profile translation, status/events/errors, and approved device operations.
- Adapter selection must use an explicit adapter identifier. Free-text vendor labels and generic device types must not select an adapter. There is no implicit vendor-adapter, ONVIF, or other fallback.
- The first registered adapter is an **identity-only reference shell**. Until later evidence and implementation gates are approved, its discovery, health, firmware, events, PTZ, live, playback, download, and configuration operations remain **unknown/not verified**.
- Adapter outcomes must distinguish succeeded, failed, unsupported, unknown, and unavailable. Missing/unverified adapter support must not be recorded as a false offline device or failed physical command.
- Other vendors can later implement the same versioned adapter contract without edits to core dispatch logic. Each real integration still requires its own evidence, security, protocol, device/firmware, licensing, and feature approval.
- The project may use documented open standards and properly authorized vendor APIs/SDKs after verification. It must not call undocumented/private cloud endpoints, bypass activation/authentication/licensing/security controls, redistribute protected vendor code/assets without permission, or claim affiliation/certification/complete compatibility without evidence.

#### P0-02 Implementation Boundary

| Included now | Deliberately deferred |
| :--- | :--- |
| Versioned adapter manifest and operation contract | Production domain/schema migration and persisted adapter assignment (P0-04/P1-04) |
| Exact registry with explicit adapter IDs and collision checks | Dynamic per-device capability negotiation and feature flags (P1-07) |
| Async normalized operation outcomes | Worker/service/event topology (P1-05) |
| Identity-only reference adapter shell | ONVIF or vendor-specific discovery, authentication, and channel import (P0-14/P2-03/P2-05/P2-14) |
| Unknown/unsupported/unavailable safeguards | Live media, PTZ, diagnostics, playback and downloads (P3/P4/P5) |
| Arbitrary second-adapter and no-fallback tests | Model/firmware compatibility and vendor support claims (P0-14/P12-02) |
| Explicit device port validation in the common core | Adapter-specific port suggestions and onboarding UX (P2-01/P2-11) |

#### Third-Party Naming and Licensing Boundary

All third-party names and marks belong to their respective owners. This independent project makes no affiliation, endorsement, certification, or compatibility claim without documented evidence. Exact names are recorded only when required to identify an approved adapter, standard, source, entitlement, or compatibility result. The repository records an open-source intention but has no selected `LICENSE`; license selection and third-party/SDK redistribution review remain later owner decisions under P0-17.

#### P0-02 Verification Checklist - 2026-07-15

- [x] Adapter selection requires an explicit adapter ID; free-text vendor and device type are ignored by dispatch.
- [x] The identity-only reference shell is the first and only default registered adapter and declares no verified operations.
- [x] Missing or unknown adapter IDs never fall back to a vendor adapter, ONVIF, or another protocol.
- [x] Version/ID/alias/operation validation rejects malformed or colliding adapters.
- [x] Operation results distinguish succeeded, failed, unsupported, unknown, and unavailable.
- [x] Unregistered or unverified operations do not create false offline health records or failed physical-command records.
- [x] An arbitrary second test adapter executes asynchronously without changes to core dispatch logic.
- [x] The shared UI/API/service layer requires and preserves an explicit device port instead of injecting port 8000.
- [x] Common device-management wording is vendor-neutral and existing vendor values are treated as metadata/demo fixtures.
- [x] Documentation makes no real vendor compatibility, private API, cloud, certification, or endorsement claim.
- [x] JavaScript syntax checks and the complete dependency-free VMS regression suite pass.
- [x] No real vendor network call, SDK, credential flow, protocol bridge, discovery, live stream, playback, download, PTZ, or firmware implementation was added under P0-02.

- [x] Discussed | [x] Approved | [x] Built | [x] Verified - **P0-03 [Architecture/NFR] Local, server, cloud, and hybrid failure behavior.** The project owner authorized adoption of the proposed decisions where standards-compatible and required replaceable modular implementation. The corrected contract, versioned policy layer, current-prototype honesty safeguards, focused tests, browser verification, and full regression suite are complete.

### P0-03 Accepted Failure-Behavior Contract

> The normative contract is maintained in [`docs/failure-behavior.md`](docs/failure-behavior.md). P0-03 defines behavior and replaceable policy boundaries. It does not claim that later-gate collectors, databases, queues, media bridges, authentication, access control, clusters, or VMS-owned recording already exist.

#### Owner Authorization and Standards Corrections

- The owner authorized the recommended P0-03 decision whenever it is compatible with standard implementation practice and asked that any conflict be reported.
- No proposed direction was prohibited by the reviewed standards. Six overly absolute formulations were corrected before implementation:
  - Continuous VMS monitoring requires a verified source-near continuity provider, not a hard-coded separate site-agent process.
  - The VMS must not synthesize or replay a late access grant; the controller follows its separately approved local/offline/life-safety policy after an authorization timeout.
  - A VMS ingestion ID prevents duplicate delivery after first acceptance but cannot prove that similar source events without a stable source ID are the same occurrence.
  - Blocking unauditable privileged/physical actions is the configurable secure default, not a universal standards mandate; autonomous life-safety and device operation remain non-blockable.
  - Health is multidimensional; stale last-known data must not be presented as current online evidence.
  - Retry rules are shared, but scheduling and budgets may be distributed per site/dependency instead of creating one global bottleneck.
- A locally calculated download hash proves later local integrity only. Equality with the source requires a trusted source digest or strong stable validator.

| Sub-item | Topic | Approved decision |
| :--- | :--- | :--- |
| P0-03A | Product and recording boundary | The VMS is an optional data-acquisition, normalization, manipulation, presentation, and interaction layer. Existing cameras, NVRs, DVRs, storage, and controllers remain autonomous. A VMS outage does not itself stop their native functions. Current continuous footage remains recorder/device owned. Optional local/remote/distributed VMS recording is deliberately deferred and must later plug in through a replaceable recording provider. |
| P0-03B | Failure isolation and continuity | Failure isolation applies inside the VMS: one failed site, device, adapter, malformed response, stream, import, or job must not unnecessarily block or falsify unrelated VMS work. Continuous VMS-side collection while the UI/WAN/coordinator is unavailable requires a declared continuity provider; its eventual process topology remains a later gate. |
| P0-03C | Authority, inventory, and configuration | A last-known-good inventory may remain visible with explicit freshness and read-only status. Writes requiring an unavailable authority must fail explicitly and must not be silently queued or reported saved. Offline conflict resolution and database technology remain P0-04 work. |
| P0-03D | Live, playback, search, and download | Media is opened only when required and when its real source/path/capability/authorization is available. Recorder-owned recording remains independent. Unreachable sources make recording state unknown unless direct fresh evidence proves otherwise. Transfer resume is capability- and validator-gated; incomplete or unverified files are never presented as complete evidence. |
| P0-03E | Commands and access | Read-only operations may use bounded safe retries. Physical, momentary, or destructive commands are live-only, expire, are never queued for reconnect replay, and report `outcome unknown` after ambiguous delivery. Controller-local/offline/life-safety behavior remains authoritative and separately approval-gated. |
| P0-03F | Alarms and audit | Future alarm/audit continuity uses durable acceptance, stable ingestion identity, source identity/sequence where supplied, at-least-once delivery, duplicate-safe central ingestion, original/receive/sync times, delayed labels, and explicit possible-duplicate/gap markers. Secure audit failure defaults may block privileged/physical VMS actions but remain configurable; viewing, autonomous recording, egress, and life-safety are not blocked by VMS audit failure. |
| P0-03G | Health and root cause | Reachability, freshness, functional health, adapter/capability state, recording, and storage remain separate dimensions. Parent unreachability makes dependent observations unknown/stale, not proven offline or not-recording. One root-cause incident represents a shared failure instead of generating a false alarm per child. |
| P0-03H | Resource pressure and recovery | Registered inventory does not imply active connections or streams. A protected system-critical lane precedes high/medium/low/idle work. Overload sheds idle/background/transfers before eligible interactive media and preserves alarms/audit capacity. Recovery uses bounded retry classification, injected timing/capacity limits, jitter, per-site/dependency budgets, and ordered restoration without delayed unsafe-command replay. |
| P0-03I | Optional services and licences | WAN, cloud, vendor-service, or optional licensed-component failure affects only functions declaring that dependency. Direct local/vendor-neutral paths continue when their actual dependencies remain available. The open-source core has no expiry or mandatory licence service, and optional-component failure must not lock inventory or evidence. |
| P0-03J | Modularity and later replacement | Availability, operation safety, health, recovery, and resource policies use stable versioned contracts with dependency injection and pure state calculation. Concrete storage, transport, workers, adapters, UI, and services can later be replaced without changing callers or core policy IDs. Unknown future functions default conservatively until explicitly classified. |

#### P0-03 Implementation Boundary

| Included now | Deliberately deferred |
| :--- | :--- |
| Versioned `shared/resilience/` contract and replaceable availability registry | Production database, authority, synchronization, snapshot, backup, or conflict implementation (P0-04/P1-10) |
| Explicit operation-safety, retry, queue, expiry, audit, and ambiguous-outcome policies | Authentication, offline-session, RBAC, credential, and final audit-storage implementation (P0-16/P1-02/P1-03/P1-06) |
| Multidimensional health derivation and root-cause helpers | Real device polling, alarm collection, device history, and final health persistence/UI domain (P1-08/P1-09/P2-13/P6) |
| Pure recovery ordering, distributed budget keys, injected full-jitter calculation, and transfer-resume decisions | Automatic retry workers, queues, coordinators, site-process topology, and numeric retry/capacity defaults (P1-05/P7-03/P12-03) |
| System-critical/high/medium/low/idle work ordering and staged degradation contract | Production auto-tuning, measured limits, decoder/network budgets, and full priority scheduler (P0-05 through P0-13/P12-03) |
| Injectable `backend/resilience/` facade | Real cloud/vendor services, access controllers, media bridges, playback/download engines, clusters, and failover (P2 through P5/P8/P12-04) |
| Honest prototype safeguards: unknown defaults, no inherited child-offline claim, no fake PTZ queue, visible inventory authority/save states, durable-save failure reporting | VMS-owned local/remote/distributed continuous recording and its storage/reconciliation/failover design (later recording/storage gates) |

#### P0-03 Verification Checklist - 2026-07-17

- [x] Native device operation and recorder-owned recording remain explicitly outside the VMS failure path.
- [x] A missing continuity provider disables continuous VMS monitoring without implying any physical-device failure.
- [x] Cached inventory becomes stale/read-only when its authority is unavailable; dependent writes are blocked explicitly.
- [x] WAN/coordinator/vendor-service/licence failure affects only functions that declare those dependencies.
- [x] Physical/destructive operations cannot be queued or replayed; expiry and ambiguous `outcome unknown` behavior are explicit.
- [x] The secure audit-failure default is injectable and replaceable rather than hard-coded as a universal standard.
- [x] Health tests keep reachability, freshness, function, recording, storage, and adapter state separate.
- [x] Parent failure makes child state unknown/stale, preserves unrelated trees, and produces one root-cause record.
- [x] Recovery and resource policies preserve system-critical work, use distributed budget keys, and accept injected numeric limits.
- [x] Download resume requires verified capability and stable source identity; local hashing does not overclaim source equality.
- [x] Adapter operations can distinguish ordinary failure from a post-send unknown outcome.
- [x] Frontend PTZ reports unavailable instead of queued until a verified adapter operation exists.
- [x] New/generated device and camera status defaults are unknown; recorder status is not copied as direct camera evidence.
- [x] Failed server-backed inventory persistence preserves an explicit unsaved draft, pauses further edits, and never claims a confirmed save.
- [x] Automatic backend persistence fallback cannot report a successful service mutation or durable inventory save.
- [x] README and backend documentation describe implemented policy boundaries without claiming deferred services.
- [x] JavaScript syntax checks, focused P0-03 policy/integration tests, the complete dependency-free VMS regression suite, and local browser verification pass.

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-04 [Architecture] Authoritative data and persistence.** Select an embedded local relational store, central database, cloud service, or synchronized edge/server ownership; define migrations, conflict resolution, ordered duplicate-safe offline reconciliation, backup, restore, and export. SQLite remains one candidate, not a decision.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-05 [NFR] Supported workstation and codec profile.** Approve operating-system families, CPU/RAM/GPU baselines, hardware acceleration, H.264/H.265 behavior, any vendor-specific codec extensions, resolution/frame-rate assumptions, and graceful degradation as decode capacity is approached. Establish profiles through current measurements rather than copied legacy hardware tables.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-06 [NFR] Local device and media capacity profile.** Define and measure supported local encoder, storage service, media service, camera, group, per-group channel, decoder, and concurrent-media limits for named deployment profiles.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-07 [NFR] Local operator and display capacity profile.** Define and measure standard/wide-screen layouts, auxiliary displays, synchronized-playback channels, active users, and map capacity for each supported workstation profile.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-08 [NFR] Local access and security capacity profile.** Define and measure organization depth, person and credential counts, access groups, authorization templates, and security zones after the governing data and privacy rules are approved.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-09 [NFR] Enterprise video capacity profile.** Define independently tested limits for directly managed devices, cameras, remote-site cameras, specialized camera interfaces, concurrent operators, and active media work.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-10 [NFR] Enterprise endpoint and security capacity profile.** Define independently tested limits for intercom, signage, visitor, patrol, alarm I/O, radar, security-area/zone, face-reader, and vehicle-recognition endpoints without inheriting a vendor product ceiling.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-11 [NFR] Enterprise analytics and event capacity profile.** Define independently tested analytics-channel, active-rule, attachment, and alarm-event throughput profiles, including overload behavior and honest degradation.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-12 [NFR/Policy] Enterprise identity and biometric capacity profile.** Define independently tested portrait, match-throughput, access-profile, door, schedule, and card-layout limits only after biometric and access-data governance is approved.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-13 [Architecture/Commercial] Separate four kinds of limits.** Model technical maximums, purchased entitlements, configured tenant/site quotas, and independently validated deployment profiles as different values.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-14 [Integration] Vendor evidence gate.** For every device adapter, cloud-managed service, enterprise VMS, remote support portal, third-party map provider, ARC, CVR, SAN, or third-party database capability, record the supported API/SDK/protocol, version, rate limits, device firmware, license, and source verification date.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-15 [Policy/Commercial] Countries, service regions, and data residency.** Confirm target countries and service regions in writing; verify data residency, cross-border transfer, telecommunications, export, subscription, and feature-availability constraints for each approved dependency. Do not repeat source-guide regional claims without dated evidence.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-16 [Policy] Security, privacy, and safety baseline.** Approve handling for video, audio, credentials, faces/biometrics, GPS, ANPR, attendance, temperature/mask data, X-ray imagery, remote unlock/barrier actions, broadcasting, firmware, password reset, retention, deletion, legal hold, and cross-border transfer.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-17 [Commercial] Licensing and entitlement scope.** Decide whether the product needs license enforcement or only deployment documentation for video, access control, intercom, visitor, attendance, mustering, remote-site, mobile, signage, video-wall, vehicle-recognition, face/body, analytics/reporting, evidence, augmented-reality, temperature/mask, security-inspection, support, and bundle capabilities.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-18 [Benefit/NFR] Measurable product outcomes.** Convert claims such as one unified interface, less empty-footage review, low latency, data sovereignty, no customer-managed central server, no VPN/public IP, lower capital cost, predictable operating cost, proactive support, and no-recurring-fee local operation into measurable, topology-specific acceptance criteria.

## Phase 1 - Core Platform Foundation

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-01 [Feature] Tenant, customer, site, area, organization, and group hierarchy.** Define ownership, nesting, movement, deletion, and cross-site visibility.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-02 [Feature/Policy] Users, super user, technician subaccounts, roles, and least privilege.** Include site-scoped permissions, privileged-action separation, activity tracking, revocation, and session expiry.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-03 [Feature/Policy] Authentication and privileged confirmation.** Define password policy, MFA or step-up authentication, recovery, lockout, service accounts, secrets storage, and approval for dangerous remote actions.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-04 [Architecture] Domain model and database migrations.** Define stable identifiers and lifecycle rules for devices, cameras, channels, groups, streams, sites, users, events, recordings, persons, credentials, doors, maps, licenses, and integrations.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-05 [Architecture] API, adapter, event-bus, and background-job boundaries.** Keep device protocols, UI, persistence, alarms, schedules, analytics, and vendor services independently testable.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-06 [Feature/Policy] Immutable audit history.** Record logins, configuration changes, live/export access, credential and biometric use, remote commands, acknowledgements, vendor calls, entitlement changes, and before/after values with trusted timestamps.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-07 [Architecture] Configuration, capability negotiation, and feature flags.** Unsupported features must be visibly unavailable; risky/vendor capabilities need scoped flags and rollback controls.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-08 [NFR] Time, time zone, clock drift, and localization.** Define machine instants, site-local wall time, operator input/display formats, daylight-saving behavior, device synchronization, ambiguous/nonexistent local times, and ordering of offline events. Existing `YYYY-MM-DD`, `HH:MM:SS`, and `YYYY-MM-DD HH:MM:SS` text are prototype proposals, not an approved universal storage format.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-09 [NFR] Observability and diagnostics.** Define logs, metrics, traces, health states, correlation IDs, operator-facing errors, maintenance suppression, and redaction of secrets/personal data.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-10 [Feature] Safe configuration import, export, backup, and recovery.** Define validation, preview, atomic commit, version compatibility, partial failure, and rollback.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-11 [Feature/Architecture] Canonical entity, location, tag, and alias data.** Define stable entity/site and location/area identities, many-to-many camera coverage, searchable tags, typed aliases, merge/history behavior, snapshots, cross-site visibility, and API/storage mappings without requiring one physical central database.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-12 [Architecture] Unified command envelope and executor.** Route UI clicks, gestures, keyboard actions, deterministic voice parsing, and any later assistant through one validated command contract with capability checks, target resolution, confirmation, result mapping, and audit boundaries.

## Phase 2 - Device, Channel, and Site Onboarding

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P2-01 [Feature] Recorder and direct-camera onboarding.** Approve workflows and required fields for NVR, DVR, hybrid DVR, and direct IP camera, including secure credentials and one-channel direct-camera behavior.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P2-02 [Feature] Automatic device group.** On every successful NVR, DVR, hybrid DVR, or direct IP-camera add, create or reconcile exactly one protected default group tied to the stable device ID and containing that device's current camera channels.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P2-03 [Feature/Integration] LAN discovery and activation.** Define autodetection, duplicate detection, activation state, supported subnets, authorization, safe bulk selection, and discovery timeout.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P2-04 [Feature/Integration] Manual, profile-import, P2P, remote-site, and cloud onboarding.** Make connection method and telemetry limitations explicit; require entitlement and site authorization where applicable.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P2-05 [Feature] Channel and stream-profile synchronization.** Add, update, reorder, rename, disable, and remove channels idempotently while preserving stable camera IDs and operator-customized metadata.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P2-06 [Feature] Rename, rescan, detach, and delete behavior.** Preserve associations on rename, detach configured cameras safely, clean stale group/routes, handle reduced channel counts, and require confirmation for destructive effects.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P2-07 [Integration] Camera capability families.** Standard, fisheye, panoramic, PTZ, thermal, ANPR/LPR, face/body, people-counting, queue, heat-map, passenger-counting, solar, and third-party cameras must advertise only proven capabilities.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P2-08 [Integration] Infrastructure devices.** Support the approved subset of storage servers, stream-media servers, CVR/SAN endpoints, decoders, video-wall outputs, auxiliary displays, and recording servers.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P2-09 [Integration] Physical-security endpoints.** Support the approved subset of doors, controllers, readers, turnstiles, face readers, alarm panels, inputs/outputs, radars, intercom door/indoor stations, speakers, signage, visitor kiosks, patrol docks, X-ray scanners, and metal detectors.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P2-10 [Integration] Mobile and field devices.** Support the approved subset of body-worn cameras, mobile/vehicular DVRs, GPS telemetry, solar cameras, and cellular data units.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P2-11 [Feature/Integration] Network configuration.** Define IP, ports, DNS, DDNS, proxy, NAT/P2P, certificates, connectivity tests, configuration validation, and authorization boundaries.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P2-12 [Integration/Policy] Remote support actions.** Separately approve log retrieval, password reset, configuration change, reboot, and batch firmware update with site authorization, preflight checks, step-up authentication, progress, failure recovery, and rollback.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P2-13 [Feature] Device health and lifecycle.** Define online/offline/degraded/unsupported states, storage and stream health, exceptions, firmware inventory, maintenance windows, notification, acknowledgement, and clear/recovery behavior.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P2-14 [Integration] Interoperability.** Decide ONVIF profiles and any other approved standards plus explicit vendor adapters; document supported profiles, models, firmware, authentication, conformance evidence, and behavior when proprietary features are unavailable.

## Phase 3 - Live Monitoring, Display, and Operator Response

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-01 [Feature] Live-view workspace and custom layouts.** Define drag/drop, saved/personal/shared layouts, camera/group/site navigation, empty/error states, and standard/wide-screen window divisions.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-02 [Feature/NFR] Multi-display operation.** Define the supported local and browser auxiliary-display profile, focus, pop-out recovery, bandwidth/decode budgets, and persisted placement through measured workstation testing.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-03 [Feature/NFR] Stream selection and decode governor.** Choose main/sub/adaptive streams, latency targets, reconnect policy, hardware acceleration, resource limits, and operator warnings before overload.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-04 [Feature] Live diagnostics.** Show bitrate, frame rate, resolution, codec, latency, packet loss, and stream state where supported; display an explicit unavailable state for P2P/cloud paths that do not provide telemetry.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-05 [Feature] Instant playback from live view.** Define look-back duration, storage source, permissions, transition back to live, and missing-footage behavior.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-06 [Feature/Integration] Fisheye display and dewarping.** Approve Panorama, PTZ, Half Sphere, AR Half Sphere, and Cylinder modes, client/device-side processing, controls, and performance limits.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-07 [Feature/Integration] PTZ and panoramic master-slave control.** Define manual PTZ, presets, tours, permissions, ownership conflicts, panoramic-to-PTZ coordination, and audit.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-08 [Feature/Integration] Target tracking.** Define supported devices/analytics, start/stop/manual override, lost-target behavior, false-positive handling, and visible tracking state.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-09 [Feature/Integration/Policy] Auxiliary device controls.** Separately approve strobe, hazardous-gas routine, and de-icing heater actions, including capability checks, interlocks, confirmation, timeout, feedback, and audit.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-10 [Feature/Integration/Policy] Live audio, intercom talk, and operator announcements.** Define half/full duplex, device selection, echo/feedback behavior, authorization, recording/consent, emergency priority, and audit.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-11 [Feature/Integration/Policy] Scheduled and event-driven mass broadcast.** Define speaker groups, content approval, schedules, priority/preemption, failed endpoints, cancellation, and safety controls.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-12 [Feature/Integration] Video walls and physical decoder outputs.** Define video-wall layouts, decoder/output routing, auxiliary GPU rendering, alarm-feed takeover, restoration, and hardware resource validation.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-13 [Feature/Integration] Digital signage.** Define display/touchscreen inventory, playlists, schedules, publishing, preview, offline cache, emergency override, and endpoint limits.
- [x] Discussed | [x] Approved | [ ] Built | [ ] Verified - **P3-14 [Feature/Architecture] Shared layered Live and Playback workspace UI.** Use two independently stateful instances of one responsive component family. The camera grid fills the workspace background; resource browser, camera/group search, selected-camera information/control, tabs, and toolbars layer over it; the application rail remains outside. Playback adds its range, timeline, transport, availability, and evidence controls without forking shared components.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-15 [Feature] Spatial site navigation and viewport media policy.** Define premises/floor canvases, camera position/facing/field of view, pan/zoom, visible/focus/peripheral/offscreen behavior, pre-warm limits, map pins versus video tiles, persistence, and performance acceptance.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-16 [Feature] Operator-led tracking sessions and breadcrumbs.** Define case type, subject descriptor, priority, active/paused/resolved state, concurrent cases, camera/time breadcrumbs, notes, resume, escalation, incident linkage, permissions, retention, and audit.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-17 [Feature/Integration/Policy] Manual route graph and camera-candidate hints.** Let operators record directional transitions and show a small ranked candidate set using site geometry and approved history. Any re-identification or predictive hint remains optional, confidence-labeled, human-reviewed, privacy-gated, and never an automatic source of truth.

## Phase 4 - Recording, Storage, Retention, and Recovery

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P4-01 [Feature] Main-stream and sub-stream recording profiles.** Define codec, resolution, frame rate, bitrate, audio, source capability, synchronized dual-stream behavior, and storage estimates.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P4-02 [Feature] Continuous, event-driven, and command-triggered schedules.** Define calendar precedence, holidays, pre/post event buffers, manual override, conflicts, and audit.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P4-03 [Feature/Architecture] Local recording and recording-server ownership.** Define edge/NVR/server/workstation destinations, gaps, index authority, failover, and retrieval priority.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P4-04 [Integration] CVR and SAN allocation.** Define provisioning, quotas, disk pools, compatibility, capacity forecasts, failure states, and safe reallocation.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P4-05 [Feature] Main and auxiliary storage.** Define primary/secondary routing, replication or independent roles, health, failover, restore, and reconciliation.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P4-06 [Feature/Policy] Retention loops and deletion.** Define per-camera/event retention, protected evidence, overwrite order, legal hold, low-space behavior, deletion proof, and time-based cleanup.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P4-07 [Integration/Policy] Encrypted cloud recording.** Separately approve event-only and continuous recording, 7/30-day or custom loops, transport/at-rest encryption, key ownership, region, export, restore, deletion, and outage behavior.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P4-08 [Integration] Cloud backup for direct IP, solar, and NVR sources.** Define supported upload paths, bandwidth control, retries, duplicate prevention, missing segments, and entitlement behavior.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P4-09 [NFR] Storage health and resilience.** Detect disk/server/cloud failures, forecast exhaustion, suppress maintenance noise, alert within the agreed SLO, recover without silent loss, and expose recording gaps.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P4-10 [NFR] Backup, restore, migration, and vendor exit.** Prove metadata/configuration backup, recording/evidence export, integrity checks, migration across versions/topologies, and recovery objectives.

## Phase 5 - Playback, Search, and Digital Evidence

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-01 [Feature/NFR] Synchronized remote playback.** Define the validated concurrent-channel profile, audio sync, scrubbing, speed, frame-step, buffering, drift tolerance, and partial-camera failure.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-02 [Feature] Playback range, footage timeline, and recording-source selection.** Keep start/end date-time selection and draggable navigation; merge continuous/event segments and local/recorder/server/cloud sources while distinguishing requested, available, missing, unknown, overlapping, and duplicate footage instead of hiding gaps or presenting requested time as recorded.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-03 [Feature] Standard, alarm-input, and event search.** Define filters, site/camera/time scoping, pagination, saved searches, result previews, and permissions.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-04 [Feature/Integration] ATM, POS, and VCA retrieval.** Define source adapters, transaction/rule indexing, time correlation, data redaction, retention, and failure states.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-05 [Feature/Integration] Human and vehicle event filtering.** Define metadata source, confidence threshold, false-positive behavior, unsupported-camera state, and measurable time saved versus manual review.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-06 [Feature/Integration/Policy] Face, body, and ANPR search.** Define authorized purpose, indexes, query fields, confidence, list matching, audit, retention, and manual fallback.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-07 [Feature/Integration/Policy] Natural-language and metadata search.** Define voice, text, and target-class search requirements; confirm actual API/service availability, supported languages, measurable accuracy/latency, explainability, privacy, entitlement, and deterministic non-AI fallback.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-08 [Feature/Policy] Incident and evidence management.** Define secure archive, case association, tag generation, bookmarks, notes, file locking, legal hold, access review, and retention.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-09 [Feature/Policy] Evidence export and chain of custody.** Define clip/snapshot formats, watermark/hash/signature, transcoding, redaction, player packaging, authorization, export audit, and integrity verification.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-10 [Feature] Reverse time/location search and playback staging.** Search a site, entity, location, area, tag, and time range; resolve all covering cameras; open them in synchronized Playback; promote a found moment into an operator tracking session; and preserve query-to-playback evidence without assuming footage exists.

## Phase 6 - Events, Alarms, Verification, and Automation

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-01 [Feature] Security areas, partitions, and zones.** Define hierarchy, arming states, bypass, ownership, schedules, permissions, and device synchronization.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-02 [Integration] Alarm inputs and outputs.** Define normalization, debounce, restore/tamper states, output duration, feedback, device loss, and safe manual control.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-03 [Feature/NFR] Event and alarm rule engine.** Define triggers, conditions, schedules, suppression, correlation, priorities, dependencies, simulation, versioning, and an independently measured active-rule profile.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-04 [NFR] Alarm ingestion and storage pipeline.** Define attachment behavior, backpressure, ordering, deduplication, persistence, recovery, and an independently measured event-throughput profile.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-05 [Feature/Policy] Automated linkage.** Separately approve camera pop-up/recording, PTZ, door, barrier, alarm output, strobe, speaker, signage, notification, and external dispatch actions with loop prevention and fail-safe defaults.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-06 [Feature] Alarm verification workspace.** Define related live/recorded video, audio, map, access/ANPR context, operator checklist, acknowledgement, escalation, resolution, and evidence capture.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-07 [Feature/NFR] Alarm lifecycle and notifications.** Define new/acknowledged/escalated/cleared states, assignment, deduplication, maintenance suppression, delivery channels, retry, SLA/SLO, and audit.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-08 [Integration] Health events and linkage.** Normalize device/storage/stream/firmware exceptions, notify and clear reliably, and enforce country/entitlement restrictions on automated actions.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-09 [Integration/Policy] Standard ARC receiver.** Confirm protocol/API, panel mapping, heartbeat, acknowledgement, duplicate/event ordering, authorization, tenancy, and audit.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-10 [Integration/Policy] Premium ARC, video verification, and dispatch.** Define evidence packet, operator confirmation, dispatch contract, failure/escalation, jurisdiction, and strict fail-closed behavior.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-11 [Feature/Policy] Compliance types, events, review logs, and overlays.** Define preset/freeform type governance, entity/location association, extracted versus operator-confirmed records, historical camera/location snapshots, manager responses, top-occurrence overlays, reporting windows, permissions, privacy, retention, and audit.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-12 [Feature/Integration/Policy] External-message ingestion and query tickets.** Through an approved connector, convert designated messages into traceable tickets with raw-source reference, entity/location, requested range, subject, confidence, status, authorization, correction, deduplication, retention, and outage behavior. No private endpoint or direct multi-process database writer is permitted.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-13 [Feature] Ticket-driven playback, tracking, and resolution.** Resolve mapped cameras, stage the requested range in Playback, link an operator tracking session and breadcrumbs, record findings and manager response, then resolve or escalate the ticket without losing provenance.

## Phase 7 - Maps, Remote Sites, AR, and Mobile Operations

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P7-01 [Feature] E-Maps.** Define image/floor-plan upload, camera/door/alarm placement, layers, status, navigation, permissions, versioning, and an independently measured map-capacity profile.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P7-02 [Feature/Integration] GIS site and camera mapping.** Define map provider, exact/approximate GPS, clustering, live status, privacy, offline behavior, rate limits, and map-license terms.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P7-03 [Feature/Integration/NFR] Remote Site Management.** Define site federation, delegated administration, event/video routing, health, time zones, entitlement, isolation, partitioning, and independently measured site/camera scale profiles.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P7-04 [Integration] P2P/cloud-connected cameras.** Define authorization, connection state, privacy, entitlement, reconnect, limited telemetry, bandwidth, and local fallback without assuming a public API.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P7-05 [Feature/Integration] AR high-point scenes.** Define scene calibration, adjacent camera/gate tags, visual overlays, interactions, permissions, per-scene entitlement, and fallback when calibration is invalid.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P7-06 [Feature/Integration/Policy] Mobile recorder and vehicle mapping.** Define GPS update rate, history, privacy, device status, playback correlation, geofences, and loss-of-signal behavior.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P7-07 [Feature/Integration/Policy] Route, speed, and driver analytics.** Define route planning, deviation, speed thresholds, driver identity/behavior, alerts, retention, labor/privacy rules, and dispute workflow.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P7-08 [Integration] Patrol docking stations.** Confirm actual workflow/API, evidence ingest, officer/device assignment, charging/upload status, exceptions, retention, and the approved endpoint count.

## Phase 8 - Access Control, Intercom, Visitors, Attendance, and Vehicles

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P8-01 [Feature/Policy] Person and credential lifecycle.** Define enrollment, approval, activation, expiry, suspension, deletion, deduplication, consent, import/sync, and audit.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P8-02 [Feature/Integration/Policy] Credential types.** Separately approve cards, reader PINs, fingerprints, face, iris, Bluetooth, dynamic QR, NFC, and mobile credentials with fallback and anti-sharing controls.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P8-03 [Feature] Doors, readers, controllers, access groups, schedules, and card templates.** Define topology, permissions, holidays, conflicts, preview, deployment, versioning, and edge synchronization.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P8-04 [Feature/Integration/Policy] Remote and mobile unlock.** Define authorized roles, step-up confirmation, reason capture, door state/feedback, timeout, retries, emergency behavior, notification, and audit.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P8-05 [NFR/Policy] Offline edge access.** Define approved outage duration, cached credential security, clock behavior, revocation limits, local audit capacity, complete ordered reconciliation, and conflict handling.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P8-06 [Feature/Policy] Advanced door logic.** Separately approve multi-door interlocking, anti-passback, first-person-in, and custom Wiegand mapping with simulations, override, fail-safe/fail-secure choices, and safety review.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P8-07 [Feature/Integration] Video intercom.** Define door/indoor stations, direct call routing, door-to-app two-way calls, missed calls, privacy, video/audio handling, remote release, and event linkage.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P8-08 [Feature/Policy] Visitor management.** Define preregistration, identity checks, host approval, self-service kiosk, badge printing, temporary credentials, escort/watchlist policy, checkout, retention, and reporting.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P8-09 [Feature/Policy] Attendance.** Define shifts, clock events, compliance, vacation approval, reports, corrections, third-party HR/database sync, privacy, and separation from access authorization.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P8-10 [Feature/Policy] GPS/mobile attendance verification.** Define geofences, spoofing controls, user consent, video/access correlation, offline handling, disputes, retention, and jurisdiction.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P8-11 [Feature/Policy] Emergency mustering.** Define fire-release routing, muster lists/checklists, authoritative occupancy, offline operation, manual override, safety certification, drills, and post-event evidence.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P8-12 [Feature/Integration/Policy] ANPR vehicle access and parking.** Define reads/confidence, allow/block lists, temporary passes, remote barrier action, search, parking/payment calculations, anti-tailgate, manual override, audit, and retention.

## Phase 9 - Analytics, Reporting, and Specialized Sensors

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-01 [Feature/Integration/Policy] Face recognition.** Define enrollment/comparison purpose, groups, thresholds, watchlists, false-match handling, human confirmation, demographic fields, audit, retention, and legal approval.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-02 [NFR/Policy] Face database and match pipeline.** Define storage/partitioning, encryption, deletion, quality controls, and independently measured portrait-capacity and match-throughput profiles.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-03 [Feature/Integration/Policy] Body-attribute matching.** Define supported attributes, prohibited inference, confidence, human review, retention, audit, and measurable bias/accuracy criteria.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-04 [Feature/Integration] People counting.** Define zones/lines, occupancy, reset/reconciliation, accuracy, privacy, reporting, and device/server analytics ownership.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-05 [Feature/Integration] Queue analytics.** Define queue zones, waiting-time thresholds, alerts, aggregation, accuracy, privacy, and operational reporting.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-06 [Feature/Integration] Heat maps, crowd flow, and BI reports.** Define inputs, aggregation, dashboards, comparisons, export, accuracy, retention, and store/site privacy.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-07 [Feature/Integration] Passenger counting.** Define stereo-camera support, entry/exit reconciliation, route/time aggregation, accuracy, missing-data handling, and reports.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-08 [Feature/Integration/Policy] Thermal, temperature, and mask analytics.** Define calibration, anomaly thresholds, trend reports, violations, access linkage, false readings, health-data privacy, and regional legality.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-09 [Feature/Integration] Radar and PTZ-radar tracking.** Define target/event model, camera handoff, zones, calibration, false alarms, degraded state, and supported device count.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-10 [Feature/Integration/Policy] Security inspection.** Define X-ray imagery, item counts, metal-detector events, operator decisions, evidence, privacy, retention, hardware/API support, and safety boundaries.

## Phase 10 - Cloud and Integrator-Service Capabilities

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P10-01 [Integration] Optional cloud-managed multi-site and multi-user tenancy.** Compare self-hosted coordination with any approved cloud-managed service; define provider responsibilities, tenant type, device import, roles, site isolation, quotas, audit, API availability, and exit behavior.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P10-02 [Benefit/NFR] No-VPN/no-public-IP connectivity.** Prove the approved path behind real NAT, DNS, proxy, firewall, bandwidth-loss, reconnect, and throttling conditions; document all edge, WAN, and vendor-cloud dependencies.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P10-03 [Integration] Proactive technician health monitoring.** Define authorization, polling/events, SLOs, deduplication, maintenance suppression, notification, remote diagnostics, recovery clearing, and tenant isolation.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P10-04 [Integration/Policy] Remote-support site authorization.** Treat active, scoped customer authorization as a hard prerequisite for remote support; define grant, expiry, revocation, sharing, audit, and fail-closed behavior.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P10-05 [Integration] Cloud attendance service.** Confirm whether this is an external integration or an in-product module, including identity sync, shifts/reports, tenancy, API, entitlement, privacy, and outage behavior.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P10-06 [Integration/Commercial] Cellular IoT/4G management.** Confirm provider/API, SIM activation, plans/usage, alerts, suspend/resume, device binding, roaming, regional support, billing, and security.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P10-07 [NFR] Cloud and license outage continuity.** Define grace periods and prove which local inventory, live view, recording, access, and alarms continue when the vendor service or subscription is unavailable.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P10-08 [Policy/NFR] Cloud security and vendor exit.** Verify encryption in transit/at rest, key ownership, hosting/data region, backups, restore, deletion, audit access, sub-processors, breach process, export formats, and termination behavior.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P10-09 [Policy] Periodic vendor/region revalidation.** Recheck licenses, free tiers, prices, regional availability, APIs, rate limits, device firmware, terms, and legal claims on an agreed cadence and before major releases.

## Phase 11 - Licensing, Service Operations, and Commercial Tools

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P11-01 [Commercial/Feature] Entitlement engine.** If in scope, model activation, base/add-on prerequisites, quantities, tenant/site assignment, grace, expiry, renewal, downgrade, overage, audit, and offline behavior.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P11-02 [Commercial] Metering units.** Decide how camera, P2P camera, body-camera, thermal channel, speaker, door, indoor station, remote site, mobile unit, decoder output, ANPR channel, face/body channel, face channel, and AR scene entitlements are represented.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P11-03 [Commercial] Free tier and proof of concept.** If a third-party offering is in scope, verify its current proof-of-concept entitlement; define activation, limits, expiry/change behavior, data access, and migration to/from paid service without making the tier an architecture dependency.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P11-04 [Commercial] Monthly/yearly subscriptions and renewal.** Define billing responsibility, activation delay, renewal, tax/currency, grace, failed payment, downgrade, cancellation, refunds, support, and data access after expiry.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P11-05 [Commercial] Support and maintenance terms.** Decide whether support and feature-update periods are tracked in-product or only documented; verify included periods and renewal rules for each selected supplier.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P11-06 [Commercial] Bundles and procurement catalog.** Treat video/access-control bundles and part numbers as versioned procurement data, not product functionality; define source, owner, refresh, region, and effective dates if retained.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P11-07 [Feature/Integration] Product specification selector.** Decide whether commercial product selection belongs in this VMS; if approved, define catalog source, compatibility rules, versioning, and vendor ownership.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P11-08 [Feature/Integration] Project registration, quotations, and pricing.** Decide whether these integrator-business workflows belong in scope; if approved, define customer/project records, approvals, currencies/tax, revisions, export, API, and audit.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P11-09 [Feature/Integration] Warranty and RMA tracking.** Decide whether it belongs in scope; if approved, define serial/device linkage, entitlement lookup, case status, logistics, attachments, notifications, API, and data ownership.

## Phase 12 - Verification, Pilot, Release, and Revalidation

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P12-01 [NFR] Per-item acceptance specification.** Every approved feature must have functional behavior, supported/unsupported states, permissions, error/failure behavior, measurable performance, audit evidence, and explicit non-goals before coding.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P12-02 [NFR] Device and firmware compatibility test matrix.** Cover approved NVRs, DVRs, hybrid DVRs, direct IP cameras, analytics cameras, storage/media/decoder devices, access/intercom/alarm endpoints, browsers, operating systems, and connection methods.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P12-03 [NFR] Capacity and performance certification.** Test the selected device/channel/user/layout/map/site/door/rule/analytics ceilings, decode profiles, storage load, search latency, and event throughput rather than copying vendor limits.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P12-04 [NFR] Network and resilience testing.** Cover packet loss, latency, low bandwidth, DNS/proxy/firewall/NAT, LAN/WAN/cloud outage, reconnect storms, server/device restart, database/storage failure, and clock drift.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P12-05 [Policy/NFR] Security testing.** Cover least privilege, MFA/step-up, site authorization, revocation, secrets, injection, request validation, tenancy isolation, audit integrity, encryption, and fail-closed privileged actions.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P12-06 [Policy/NFR] Privacy and safety validation.** Cover video/audio/biometric/GPS/ANPR/attendance/temperature retention, consent/notice, minimization, export/deletion, access review, remote physical actions, emergency workflows, and jurisdictional approval.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P12-07 [NFR] Evidence and recovery validation.** Prove recording gaps are visible, backups restore, exports verify, legal holds survive retention, audit trails are complete, migrations preserve IDs, and rollback does not corrupt state.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P12-08 [NFR] Accessibility, localization, and operator usability.** Validate keyboard/screen-reader support, color/status clarity, language, date/time/number formats, time zones, alarm overload, dangerous-action confirmation, and representative operator workflows.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P12-09 [NFR] Documentation and operations readiness.** Complete installation, configuration, security, privacy, backup/restore, upgrade/rollback, device compatibility, operator/admin training, monitoring, incident, vendor escalation, and support runbooks.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P12-10 [NFR] Lab, pilot, UAT, and release gates.** Require representative hardware/firmware/tenant/region lab evidence, limited-site pilot, user acceptance, monitored rollout, feature flags, migration, rollback, ownership, and final sign-off.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P12-11 [NFR/Commercial/Policy] Scheduled revalidation.** Record source versions and expiry dates, then revalidate vendor capability, API, firmware, license, region, security, privacy, and legal assumptions before major upgrades.

---

## Source Coverage and Evidence Record

The 2026-07-19 comparison with the supplied original 623-line research guide found:

- all 143 original P0-P12 feature and gate IDs present, with zero missing IDs and zero status differences;
- seven deliberately revised titles—P3-12, P5-02, P5-07, P10-01, P10-04, P11-05, and P11-06—using vendor-neutral or clearer capability wording without removing their scope;
- ten project-specific additions—P1-11, P1-12, P3-14 through P3-17, P5-10, and P6-11 through P6-13—plus the eight-item C0 consolidation track;
- source capability coverage across platform foundation and limits (P0-P2/P12), live operation (P3), recording/storage (P4), playback/evidence (P5), alarms (P6), maps/mobile (P7), access/intercom (P8), analytics (P9), optional cloud/integrator services (P10), and commercial tooling (P11);
- no vendor name retained in the controlling roadmap; exact names remain outside the repository unless a later evidence record or adapter specification requires identification.

The external guide remains outside the repository as discovery material. Its copied prose, product names, hardware tables, capacity figures, SKUs, prices/entitlements, regional claims, and bibliography are deliberately not reproduced here. Before a third-party or standards-based integration is approved, P0-14 requires a claim-level evidence record containing:

- evidence ID and capability;
- exact product/service/standard and public-safe label;
- version, mode, API/SDK/protocol/profile, device model, and firmware;
- license/entitlement, tenant, country/region, and support terms;
- primary source, source version/publication date, review date, reviewer, and confidence;
- expiry/revalidation date and the roadmap item that depends on the claim.

Feature ideas and factual systems are restated independently in this roadmap; source wording is not a product requirement. This editorial cleanup reduces avoidable third-party naming and copied-expression risk, but it is not legal clearance for a product name, integration claim, license, or distribution package.
