# VMS Development Master Checklist and Approval Plan

> **Status:** This is the controlling development roadmap. The research guide is preserved in full below it as discovery/reference material. A vendor claim in the research section is not automatically an approved requirement.

## Approval Contract

1. We will work through this checklist in dependency order, one approval item at a time unless an explicitly approved dependency requires a different order.
2. Before any product code is written for an item, Codex must discuss its problem, intended users, benefit, included and excluded scope, workflow, dependencies, data and security impact, measurable acceptance criteria, and important alternatives with the project owner.
3. The project owner must explicitly approve the agreed scope. Silence, an existing vendor feature, an existing implementation, or approval of another item does not count as approval.
4. Product code may start only when both the **Discussed** and **Approved** boxes for that item are checked and its decision is recorded in the Approval Log.
5. **Built** is checked only after implementation is complete. **Verified** is checked only after the agreed tests and acceptance criteria pass.
6. An item may instead be recorded as **Changed**, **Deferred**, **Rejected**, or **Out of scope**. Deferred or rejected work must not be implemented.
7. Benefits, capacity limits, commercial rules, regional availability, and safety/privacy claims are gates or acceptance criteria. They are not silently converted into product features.
8. Any capability depending on Reference Vendor or another vendor requires confirmation of the exact product, mode, version, device/firmware, API or SDK, license, tenant, target country, and support terms before approval.
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
| P0-02 | 2026-07-15 | Approved, built, and verified | Independent vendor-neutral VMS core with Reference Vendor as the first explicit adapter identity | Versioned async adapter contract, exact registry, Reference Vendor identity shell, explicit port validation, arbitrary-adapter/no-fallback tests, documentation audit, and full regression suite | Real vendor/protocol operations remain unapproved |
| P0-03 | 2026-07-17 | Approved, built, and verified | Standards-compatible modular failure behavior for an optional VMS data/interaction layer; autonomous source devices and recorder-owned footage; honest degraded/read-only/unknown states; non-replayable physical commands; replaceable resilience policies | Normative failure-behavior contract, standards-correction audit, shared versioned policy modules, injectable backend facade, current-prototype honesty fixes, focused policy/integration tests, browser verification, and full regression suite | Concrete collectors, queues, databases, authentication, media bridges, access controllers, clusters, and VMS-owned recording remain later gates |

## Phase 0 - Product Direction, Evidence, and Capacity Gates

- [x] Discussed | [x] Approved | [x] Built | [x] Verified - **P0-01 [Architecture/Benefit] Product and deployment goal.** The project owner explicitly approved the complete P0-01 direction on 2026-07-15. The accepted contract, documentation alignment, and Administrator-only permission-ready prototype baseline are implemented and verified. This completion does not authorize implementation belonging to later roadmap gates.

### P0-01 Approved Decision Record

> These entries are approved as the product direction. They authorize the P0-01 architecture contract only. Detailed feature and infrastructure implementation remains subject to its own later roadmap gate.

| Sub-item | Topic | Current state | Approved decision |
| :--- | :--- | :--- | :--- |
| P0-01A | First users | Accepted | Release one starts with one **Administrator** role having all available privileges. Features must be designed around stable capabilities/permission boundaries so later role separation does not require rewriting them. The role editor and feature-to-role assignment will be built after the initial feature set; strong role-based permissions remain a later requirement under P1-02/P1-03. |
| P0-01B | Site pattern and scale | Accepted | The intended architecture supports many local and remote control rooms, sites, and operators, with an eventual scale direction of hundreds of thousands of registered cameras and thousands of NVRs/DVRs. The software must not impose artificial commercial limits. Measurable release profiles and horizontal-scaling boundaries will be defined and tested later; "any number" is not an untested capacity guarantee. |
| P0-01C | Primary topology | Accepted | **Hybrid** means self-contained standalone local operation plus optional self-hosted coordinated and scale-out operation for shared multi-site/control-room use. It does not imply a required Reference Vendor/proprietary cloud subscription. Exact service placement, synchronization, and failure behavior remain later gates. |
| P0-01D | Cost and licensing | Accepted | The product is intended to be open source and must not require a Reference Vendor premium plan, mandatory proprietary cloud, recurring vendor subscription, or forced telemetry. The Reference Vendor guide is a feature-discovery checklist, not a dependency or licensing plan. The repository's specific open-source license remains a later owner decision. |
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

- [x] Discussed | [x] Approved | [x] Built | [x] Verified - **P0-02 [Architecture] Product boundary.** The project owner approved an independent, vendor-neutral core with Reference Vendor as the first adapter implementation. The versioned boundary, exact registry, identity-only Reference Vendor shell, normalized outcome safeguards, neutral port handling, documentation, and tests are implemented and verified. Real vendor/open-standard/RTSP/SDK/cloud operations remain unapproved.

### P0-02 Accepted Vendor-Boundary Contract

- The product is an independent, vendor-neutral VMS. Reference Vendor is the first prioritized adapter and reference integration, not the product identity, an exclusive dependency, or a claim of current device compatibility.
- The core owns normalized sites, devices, cameras/channels, groups, users/capability identifiers, jobs, events, health records, playback requests, evidence records, errors, UI workflows, persistence contracts, media policy, and auditing concepts.
- Optional adapters own vendor/protocol identity, authentication/session details, device/model/firmware interpretation, discovery, channel/profile translation, status/events/errors, and approved device operations.
- Adapter selection must use an explicit adapter identifier. Free-text vendor labels and generic device types must not select an adapter. There is no implicit Reference Vendor, open interoperability, or other fallback.
- Reference Vendor is registered first as an **identity-only adapter shell**. Until later evidence and implementation gates are approved, its discovery, health, firmware, events, PTZ, live, playback, download, and configuration operations remain **unknown/not verified**.
- Adapter outcomes must distinguish succeeded, failed, unsupported, unknown, and unavailable. Missing/unverified adapter support must not be recorded as a false offline device or failed physical command.
- Other vendors can later implement the same versioned adapter contract without edits to core dispatch logic. Each real integration still requires its own evidence, security, protocol, device/firmware, licensing, and feature approval.
- The project may use documented open standards and properly authorized vendor APIs/SDKs after verification. It must not call undocumented/private cloud endpoints, bypass activation/authentication/licensing/security controls, redistribute protected vendor code/assets without permission, or claim affiliation/certification/complete compatibility without evidence.

#### P0-02 Implementation Boundary

| Included now | Deliberately deferred |
| :--- | :--- |
| Versioned adapter manifest and operation contract | Production domain/schema migration and persisted adapter assignment (P0-04/P1-04) |
| Exact registry with explicit adapter IDs and collision checks | Dynamic per-device capability negotiation and feature flags (P1-07) |
| Async normalized operation outcomes | Worker/service/event topology (P1-05) |
| Reference Vendor identity-only shell | vendor/open-standard discovery, authentication and channel import (P0-14/P2-03/P2-05/P2-14) |
| Unknown/unsupported/unavailable safeguards | Live media, PTZ, diagnostics, playback and downloads (P3/P4/P5) |
| Arbitrary second-adapter and no-fallback tests | Model/firmware compatibility and vendor support claims (P0-14/P12-02) |
| Explicit device port validation in the common core | Adapter-specific port suggestions and onboarding UX (P2-01/P2-11) |

#### Trademark and Licensing Boundary

Reference Vendor and related product names are trademarks of their respective owners. This independent project is not affiliated with or endorsed by Reference Vendor. The repository currently records an open-source intention but has no selected `LICENSE`; license selection and third-party/SDK redistribution review remain later owner decisions under P0-17.

#### P0-02 Verification Checklist - 2026-07-15

- [x] Adapter selection requires an explicit adapter ID; free-text vendor and device type are ignored by dispatch.
- [x] Reference Vendor is the first and only default registered adapter identity and declares no verified operations.
- [x] Missing or unknown adapter IDs never fall back to Reference Vendor, open interoperability, or another adapter.
- [x] Version/ID/alias/operation validation rejects malformed or colliding adapters.
- [x] Operation results distinguish succeeded, failed, unsupported, unknown, and unavailable.
- [x] Unregistered or unverified operations do not create false offline health records or failed physical-command records.
- [x] An arbitrary second test adapter executes asynchronously without changes to core dispatch logic.
- [x] The shared UI/API/service layer requires and preserves an explicit device port instead of injecting port 8000.
- [x] Common device-management wording is vendor-neutral and existing vendor values are treated as metadata/demo fixtures.
- [x] Documentation makes no real Reference Vendor compatibility, private API, cloud, certification, or endorsement claim.
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

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-04 [Architecture] Authoritative data and persistence.** Select local SQLite, central database, cloud service, or synchronized edge/server ownership; define migrations, conflict resolution, ordered duplicate-safe offline reconciliation, backup, restore, and export.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-05 [NFR] Supported workstation and codec profile.** Approve operating systems, CPU/RAM/GPU baselines, hardware acceleration, H.264/H.264+/H.265/H.265+ behavior, resolution/frame-rate assumptions, and graceful degradation when decode capacity is approached. Validate the guide's old Windows 7/macOS hardware figures before using them.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-06 [NFR] Local device and media capacity profile.** Decide whether to target the guide ceilings of 256 encoders, 16 storage servers, 16 stream-media servers, 1,024 cameras, 256 groups, 64 legacy or 256 current channels per group, and 64 decoders.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-07 [NFR] Local operator and display capacity profile.** Decide whether to target 64 standard or 48 wide-screen windows, four local auxiliary displays, 16-channel synchronized playback, 50 active users plus one super user, and 256 E-maps.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-08 [NFR] Local access and security capacity profile.** Decide whether to target ten organization levels, 10,000 legacy or 3,200 current persons, five cards per person legacy or 16,000 total current cards, 128 access groups, 255 authorization templates, and 2,048 zones. Resolve the legacy/current anomalies before implementation.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-09 [NFR] Enterprise video capacity profile.** Decide whether to target 2,048 directly managed devices, 10,000 cameras or 100,000 through remote-site management, and 10,000 fisheye interfaces.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-10 [NFR] Enterprise endpoint and security capacity profile.** Decide whether to target 5,000 intercom terminals, 2,048 signage endpoints, 32 visitor kiosks, 1,500 patrol docks, 5,000 alarm inputs, 3,000 outputs, 30 radar arrays, 256 security areas, 2,048 zones, 32 DS-5600 face readers, and 3,000 ANPR channels.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-11 [NFR] Enterprise analytics and event capacity profile.** Decide whether the 300 people-counting, 300 queue, 70 heat-map, and 20 thermal figures are sizing recommendations; decide whether to target 10,000 active rules and 1,000 attachment-free alarm events per second.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-12 [NFR/Policy] Enterprise identity and biometric capacity profile.** Decide whether to target one million face portraits, 100 stored face matches per second, 50,000 access profiles, 1,024 doors, 32 schedules, and 32 card layouts; approve biometric and access-data governance before testing those limits.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-13 [Architecture/Commercial] Separate four kinds of limits.** Model technical maximums, purchased entitlements, configured tenant/site quotas, and independently validated deployment profiles as different values.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-14 [Integration] Vendor evidence gate.** For every device adapter, cloud-managed service, enterprise VMS, remote support portal, third-party map provider, ARC, CVR, SAN, or third-party database capability, record the supported API/SDK/protocol, version, rate limits, device firmware, license, and source verification date.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-15 [Policy/Commercial] Countries, service regions, and data residency.** Confirm target countries and cloud regions in writing. Revalidate the guide's US/Canada linkage restriction, listed African free-only markets, Japan/Taiwan free-only claim, and Europe/Latin America availability. Pakistan/South Asia is unresolved in the source guide.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-16 [Policy] Security, privacy, and safety baseline.** Approve handling for video, audio, credentials, faces/biometrics, GPS, ANPR, attendance, temperature/mask data, X-ray imagery, remote unlock/barrier actions, broadcasting, firmware, password reset, retention, deletion, legal hold, and cross-border transfer.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-17 [Commercial] Licensing and entitlement scope.** Decide whether the product needs license enforcement or only deployment documentation for VSS, ACS, intercom, visitor, attendance, mustering, RSM, mobile, signage, Smart Wall, ANPR, face/body, BI, evidence, AR, temperature/mask, security inspection, support, and bundles.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P0-18 [Benefit/NFR] Measurable product outcomes.** Convert claims such as one unified interface, less empty-footage review, low latency, data sovereignty, no customer-managed central server, no VPN/public IP, lower capital cost, predictable operating cost, proactive support, and no-recurring-fee local operation into measurable, topology-specific acceptance criteria.

## Phase 1 - Core Platform Foundation

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-01 [Feature] Tenant, customer, site, area, organization, and group hierarchy.** Define ownership, nesting, movement, deletion, and cross-site visibility.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-02 [Feature/Policy] Users, super user, technician subaccounts, roles, and least privilege.** Include site-scoped permissions, privileged-action separation, activity tracking, revocation, and session expiry.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-03 [Feature/Policy] Authentication and privileged confirmation.** Define password policy, MFA or step-up authentication, recovery, lockout, service accounts, secrets storage, and approval for dangerous remote actions.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-04 [Architecture] Domain model and database migrations.** Define stable identifiers and lifecycle rules for devices, cameras, channels, groups, streams, sites, users, events, recordings, persons, credentials, doors, maps, licenses, and integrations.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-05 [Architecture] API, adapter, event-bus, and background-job boundaries.** Keep device protocols, UI, persistence, alarms, schedules, analytics, and vendor services independently testable.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-06 [Feature/Policy] Immutable audit history.** Record logins, configuration changes, live/export access, credential and biometric use, remote commands, acknowledgements, vendor calls, entitlement changes, and before/after values with trusted timestamps.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-07 [Architecture] Configuration, capability negotiation, and feature flags.** Unsupported features must be visibly unavailable; risky/vendor capabilities need scoped flags and rollback controls.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-08 [NFR] Time, time zone, clock drift, and localization.** Define canonical timestamps, site-local display, daylight-saving behavior, device synchronization, and ordering of offline events.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-09 [NFR] Observability and diagnostics.** Define logs, metrics, traces, health states, correlation IDs, operator-facing errors, maintenance suppression, and redaction of secrets/personal data.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P1-10 [Feature] Safe configuration import, export, backup, and recovery.** Define validation, preview, atomic commit, version compatibility, partial failure, and rollback.

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
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P2-14 [Integration] Interoperability.** Decide open interoperability or other standard protocols and any explicit Reference Vendor/Alternate Vendor adapters; document supported models/firmware and behavior when proprietary features are unavailable.

## Phase 3 - Live Monitoring, Display, and Operator Response

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-01 [Feature] Live-view workspace and custom layouts.** Define drag/drop, saved/personal/shared layouts, camera/group/site navigation, empty/error states, and standard/wide-screen window divisions.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-02 [Feature/NFR] Multi-display operation.** Define up to four local auxiliary displays and/or three auxiliary browser displays, focus, pop-out recovery, bandwidth/decode budgets, and persisted placement.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-03 [Feature/NFR] Stream selection and decode governor.** Choose main/sub/adaptive streams, latency targets, reconnect policy, hardware acceleration, resource limits, and operator warnings before overload.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-04 [Feature] Live diagnostics.** Show bitrate, frame rate, resolution, codec, latency, packet loss, and stream state where supported; display an explicit unavailable state for P2P/cloud paths that do not provide telemetry.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-05 [Feature] Instant playback from live view.** Define look-back duration, storage source, permissions, transition back to live, and missing-footage behavior.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-06 [Feature/Integration] Fisheye display and dewarping.** Approve Panorama, PTZ, Half Sphere, AR Half Sphere, and Cylinder modes, client/device-side processing, controls, and performance limits.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-07 [Feature/Integration] PTZ and panoramic master-slave control.** Define manual PTZ, presets, tours, permissions, ownership conflicts, panoramic-to-PTZ coordination, and audit.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-08 [Feature/Integration] Target tracking.** Define supported devices/analytics, start/stop/manual override, lost-target behavior, false-positive handling, and visible tracking state.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-09 [Feature/Integration/Policy] Auxiliary device controls.** Separately approve strobe, hazardous-gas routine, and de-icing heater actions, including capability checks, interlocks, confirmation, timeout, feedback, and audit.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-10 [Feature/Integration/Policy] Live audio, intercom talk, and operator announcements.** Define half/full duplex, device selection, echo/feedback behavior, authorization, recording/consent, emergency priority, and audit.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-11 [Feature/Integration/Policy] Scheduled and event-driven mass broadcast.** Define speaker groups, content approval, schedules, priority/preemption, failed endpoints, cancellation, and safety controls.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-12 [Feature/Integration] Smart Wall and physical decoder outputs.** Define video-wall layouts, decoder/output routing, auxiliary GPU rendering, alarm-feed takeover, restoration, and hardware resource validation.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P3-13 [Feature/Integration] Digital signage.** Define display/touchscreen inventory, playlists, schedules, publishing, preview, offline cache, emergency override, and endpoint limits.

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

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-01 [Feature/NFR] Synchronized remote playback.** Define up to 16 channels, audio sync, scrubbing, speed, frame-step, buffering, drift tolerance, and partial-camera failure.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-02 [Feature] Timeline and recording-source selection.** Merge continuous/event segments and local/NVR/server/cloud sources without hiding gaps or duplicates.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-03 [Feature] Standard, alarm-input, and event search.** Define filters, site/camera/time scoping, pagination, saved searches, result previews, and permissions.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-04 [Feature/Integration] ATM, POS, and VCA retrieval.** Define source adapters, transaction/rule indexing, time correlation, data redaction, retention, and failure states.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-05 [Feature/Integration] Human and vehicle event filtering.** Define metadata source, confidence threshold, false-positive behavior, unsupported-camera state, and measurable time saved versus manual review.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-06 [Feature/Integration/Policy] Face, body, and ANPR search.** Define authorized purpose, indexes, query fields, confidence, list matching, audit, retention, and manual fallback.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-07 [Integration/Policy] semantic-search voice, text, and target-class search.** Confirm actual API/service availability, language support, accuracy/latency targets, explainability, privacy, entitlement, and non-AI fallback.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-08 [Feature/Policy] Incident and evidence management.** Define secure archive, case association, tag generation, bookmarks, notes, file locking, legal hold, access review, and retention.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P5-09 [Feature/Policy] Evidence export and chain of custody.** Define clip/snapshot formats, watermark/hash/signature, transcoding, redaction, player packaging, authorization, export audit, and integrity verification.

## Phase 6 - Events, Alarms, Verification, and Automation

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-01 [Feature] Security areas, partitions, and zones.** Define hierarchy, arming states, bypass, ownership, schedules, permissions, and device synchronization.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-02 [Integration] Alarm inputs and outputs.** Define normalization, debounce, restore/tamper states, output duration, feedback, device loss, and safe manual control.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-03 [Feature/NFR] Event and alarm rule engine.** Define triggers, conditions, schedules, suppression, correlation, priorities, dependencies, simulation, versioning, and the approved scale up to 10,000 rules.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-04 [NFR] Alarm ingestion and storage pipeline.** Define attachment behavior, backpressure, ordering, deduplication, persistence, recovery, and whether to validate 1,000 attachment-free events per second.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-05 [Feature/Policy] Automated linkage.** Separately approve camera pop-up/recording, PTZ, door, barrier, alarm output, strobe, speaker, signage, notification, and external dispatch actions with loop prevention and fail-safe defaults.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-06 [Feature] Alarm verification workspace.** Define related live/recorded video, audio, map, access/ANPR context, operator checklist, acknowledgement, escalation, resolution, and evidence capture.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-07 [Feature/NFR] Alarm lifecycle and notifications.** Define new/acknowledged/escalated/cleared states, assignment, deduplication, maintenance suppression, delivery channels, retry, SLA/SLO, and audit.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-08 [Integration] Health events and linkage.** Normalize device/storage/stream/firmware exceptions, notify and clear reliably, and enforce country/entitlement restrictions on automated actions.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-09 [Integration/Policy] Standard ARC receiver.** Confirm protocol/API, panel mapping, heartbeat, acknowledgement, duplicate/event ordering, authorization, tenancy, and audit.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P6-10 [Integration/Policy] Premium ARC, video verification, and dispatch.** Define evidence packet, operator confirmation, dispatch contract, failure/escalation, jurisdiction, and strict fail-closed behavior.

## Phase 7 - Maps, Remote Sites, AR, and Mobile Operations

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P7-01 [Feature] E-Maps.** Define image/floor-plan upload, camera/door/alarm placement, layers, status, navigation, permissions, versioning, and the approved 256-map capacity.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P7-02 [Feature/Integration] GIS site and camera mapping.** Define map provider, exact/approximate GPS, clustering, live status, privacy, offline behavior, rate limits, and map-license terms.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P7-03 [Feature/Integration/NFR] Remote Site Management.** Define site federation, delegated administration, event/video routing, health, time zones, entitlement, isolation, and approved scale from two sites toward the stated 1,024-site/100,000-camera profile.
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
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-02 [NFR/Policy] Face database and match pipeline.** Define storage/partitioning, encryption, deletion, quality controls, and whether to validate one million portraits and 100 stored matches per second.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-03 [Feature/Integration/Policy] Body-attribute matching.** Define supported attributes, prohibited inference, confidence, human review, retention, audit, and measurable bias/accuracy criteria.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-04 [Feature/Integration] People counting.** Define zones/lines, occupancy, reset/reconciliation, accuracy, privacy, reporting, and device/server analytics ownership.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-05 [Feature/Integration] Queue analytics.** Define queue zones, waiting-time thresholds, alerts, aggregation, accuracy, privacy, and operational reporting.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-06 [Feature/Integration] Heat maps, crowd flow, and BI reports.** Define inputs, aggregation, dashboards, comparisons, export, accuracy, retention, and store/site privacy.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-07 [Feature/Integration] Passenger counting.** Define stereo-camera support, entry/exit reconciliation, route/time aggregation, accuracy, missing-data handling, and reports.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-08 [Feature/Integration/Policy] Thermal, temperature, and mask analytics.** Define calibration, anomaly thresholds, trend reports, violations, access linkage, false readings, health-data privacy, and regional legality.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-09 [Feature/Integration] Radar and PTZ-radar tracking.** Define target/event model, camera handoff, zones, calibration, false alarms, degraded state, and supported device count.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P9-10 [Feature/Integration/Policy] Security inspection.** Define X-ray imagery, item counts, metal-detector events, operator decisions, evidence, privacy, retention, hardware/API support, and safety boundaries.

## Phase 10 - Cloud and Integrator-Service Capabilities

- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P10-01 [Integration] Cloud multi-site and multi-user tenancy.** Confirm cloud-managed team mode versus enterprise VMS Connect responsibilities, supported tenant type, device import, roles, site isolation, quotas, audit, and API availability.
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
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P11-03 [Commercial] Free tier and proof of concept.** Vendor-verify the stated 32 video channels and ten doors; define activation, limits, expiry/change behavior, data access, and migration to/from paid service without making the tier an architecture dependency.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P11-04 [Commercial] Monthly/yearly subscriptions and renewal.** Define billing responsibility, activation delay, renewal, tax/currency, grace, failed payment, downgrade, cancellation, refunds, support, and data access after expiry.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P11-05 [Commercial] SUP and maintenance.** Decide whether one-/three-year support and feature-update periods are tracked in-product or only documented; vendor-verify included support periods and renewal rules.
- [ ] Discussed | [ ] Approved | [ ] Built | [ ] Verified - **P11-06 [Commercial] Bundles and SKU catalog.** Treat promotional VSS/ACS bundles and part numbers as versioned procurement data, not product functionality; define source, owner, refresh, region, and effective dates if retained.
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

## Preserved Research and Vendor Reference

# **Technical Specifications and Architectural Analysis of Video Monitoring Platforms**

The execution of unified physical security relies heavily on the capabilities of the underlying software management platforms. These platforms act as central nodes that process high-bandwidth video streams, manage complex access credentials, log telemetry, and run edge-intelligent analytics. A typical portfolio may include localized utility clients, enterprise central management platforms, cloud-native Software-as-a-Service (SaaS) portals, and specialized integrator tools. This analysis provides a general technical evaluation of their features, system limits, licensing frameworks, and operational configurations.

## **Baseline Platform Architecture: desktop VMS client Series**

The desktop VMS client series client software is designed as a localized utility for configuring and managing Reference Vendor hardware. Operating on a distributed architecture utilizing a local SQLite database, the system acts as a decentralized software controller. It unifies video surveillance, access control, video intercom, security alarm partitions, storage configurations, and auxiliary decoding modules on a single workstation client.

### **Technical Specifications and Local System Demands**

Because desktop VMS client uses local decoding, its performance depends directly on the host computer's processor, memory, and graphics hardware. The software handles video stream decoding using H.264, H.264+, H.265, and H.265+ formats.  
The system's hardware requirements and corresponding decoding limits vary depending on the host machine's configuration:

| System Tier | Operating System | CPU Specification | RAM | Graphics Processing Unit | Maximum Simultaneous Live View Channels (H.264 / H.265 @ 1080p, 30fps) |
| :---- | :---- | :---- | :---- | :---- | :---- |
| **Minimum** | Windows 7 (64-bit) | Intel Core i5-4590 @ 3.30 GHz | 8 GB | NVIDIA GeForce GTX 970 | 11 Channels / 8 Channels |
| **Recommended** | Windows 7 (64-bit) | Intel Xeon E3-1226 V3 @ 3.30 GHz | 8 GB | Intel HD Graphics P4600 | 12 Channels / 8 Channels |
| **High Performance** | Windows 7 (64-bit) | Intel Core i7-6700K @ 4.00 GHz | 16 GB | NVIDIA GeForce GTX 1070 | 21 Channels / 15 Channels |
| **macOS Entry** | macOS Baseline | Intel Core i5 @ 2.00 GHz | 8 GB | Intel Iris Graphics 540 | 12 Channels / 8 Channels |
| **macOS Pro** | macOS High-Tier | Intel Core i7 @ 2.00 GHz | 4 GB | AMD Radeon HD 6490M | 8 Channels / 6 Channels |

The system's capacity limits define its suitability for small-to-medium deployments:

| Architecture Element | Maximum Parameter Capacity |
| :---- | :---- |
| **Connectable Encoding Devices** | 256 physical units |
| **Connectable Storage Servers** | 16 server endpoints |
| **Connectable Stream Media Servers** | 16 server endpoints |
| **Maximum Camera Channels** | 1,024 channels across all groups |
| **Configurable Camera Groups** | 256 distinct groups |
| **Max Channels per Group** | 64 channels (Legacy) / 256 channels (Current) |
| **Connectable Decoding Devices** | 64 physical decoders |
| **Standard Screen Window Division** | 64-window logical matrix |
| **Wide Screen Window Division** | 48-window logical matrix |
| **Auxiliary Screen Support** | 4 auxiliary displays for concurrent live view |
| **Concurrent Active User Accounts** | 50 users plus 1 super user |
| **E-Map Support Limit** | 256 independent graphic layouts |
| **Access Control Organizations** | 10 hierarchical levels |
| **Access Control Person Database** | 10,000 profile records (Legacy) / 3,200 (Current) |
| **Credential Capacity** | 5 cards per person (Legacy) / 16,000 total cards (Current) |
| **Access Groups & Templates** | 128 access groups / 255 authorization templates |
| **Security Control Panel Zones** | 2,048 programmable security zones |

### **Core Toolsets and Operational Functionalities**

The desktop VMS client series includes toolsets for managing, routing, and checking security event footage on-site:

* **Live Monitoring Engine:** Features custom video configurations and instant playback. It displays real-time stream parameters, including bitrate, frame rate, and resolution, to assist with bandwidth diagnostics (excluding devices added via P2P Cloud). It supports live viewing in fisheye mode for compatible cameras, offering dewarping layouts like Panorama, PTZ, Half Sphere, AR Half Sphere, and Cylinder views.  
* **Target Tracking and Alarm Verification:** Integrates Master-Slave target tracking to coordinate panoramic cameras with PTZ configurations. Operators can manually activate strobe lights, hazardous gas detection routines, target tracking parameters, and de-icing heaters for extreme weather environments. Operators can also transmit live audio alerts or scheduled mass broadcasts to networked IP speakers directly from the client interface.  
* **Archival and Recording Parameters:** Supports multi-stream target configurations, allowing synchronous recording of both main streams and sub-streams. It schedules recordings based on continuous, event-driven, or command-triggered parameters. For large storage systems, it supports CVR and SAN configurations to allocate hard disk space and automate retention loops.  
* **Playback and Retrieval Tools:** Offers synchronous remote playback of up to 16 channels, complete with audio sync. Operators can filter search results using standard, alarm-input, event, ATM transaction, VCA rules, or POS database indices. The software can also filter recorded footage to display only human or vehicle detection events, saving operators from reviewing hours of empty video.  
* **Access Control and Intercom Integration:** Manages doors, readers, and user credentials within a single interface. It supports multiple authorization modes, including card reader PINs, fingerprints, face profiles, and iris parameters. Advanced features include multi-door interlocking, anti-passback, first-person-in validation, and custom Wiegand protocol mapping. It also integrates video intercom door stations, supporting direct call-routing, remote door releases, and automated event linkage.

## **Enterprise Central Management: enterprise VMS platform**

For larger projects, enterprise VMS platform serves as an integrated, modular central management system (CMS). The platform uses a lightweight, web-accessible database structure designed for deep integration across video, access control, vehicle tracking, thermal monitoring, digital signage, and analytics systems.

### **Physical Scalability Limits and Operational Profiles**

enterprise VMS platform is engineered to run on enterprise-grade hardware, such as Intel Xeon processors, large DDR4 memory arrays, and hardware SAS RAID controllers. It coordinates multiple distinct subsystems to protect assets and ensure business continuity.  
The system's database handles high-speed data logging, transaction routing, and alarm processing:

| Database Parameter | Maximum Operational Limit |
| :---- | :---- |
| **Directly Managed Devices** | 2,048 network devices |
| **Max Managed Cameras** | 10,000 camera channels (Up to 100,000 when using Remote Site Management) |
| **Fisheye Camera Deployments** | 10,000 camera interfaces |
| **Video Intercom Terminals** | 5,000 indoor/outdoor station terminals |
| **Digital Signage Displays** | 2,048 signage endpoints |
| **Visitor Self-Service Terminals** | 32 physical units |
| **Security Patrol Docking Stations** | 1,500 docking terminals |
| **Alarm Inputs & Alarm Outputs** | 5,000 hardware inputs / 3,000 outputs |
| **Security Radars & PTZ Radar Units** | 30 active radar arrays |
| **Security Control Areas (Partitions)** | 256 software areas / 2,048 programmable zones |
| **Turnstile-Integrated Face Readers** | 32 units (DS-5600 Series) |
| **ANPR Camera Deployments** | 3,000 localized vehicle reading channels |
| **Recommended Intelligence Units** | 300 People Counting / 300 Queue / 70 Heat Map / 20 Thermal cameras |
| **Event and Alarm Rules** | 10,000 concurrent active rule profiles |
| **Alarm Storage Processing** | 1,000 events per second (without attachments) |
| **Face Comparison Database** | 1,000,000 portrait records across all groups |
| **Face Match Events Storage** | 100 matches per second (written to recording server) |
| **Active Access Profiles & Doors** | 50,000 access files / 1,024 physical access points |
| **Access Schedules & Card Templates** | 32 scheduling matrixes / 32 dynamic card layouts |

### **Licensing Structure, Baseline Packages, and Add-on Modules**

enterprise VMS platform uses a modular licensing framework where organizations only purchase the specific baseline software packages and add-on expansion modules their deployment requires. Base software licenses include a Software Upgrade Program (SUP) subscription, which grants access to technical support and new feature releases. The baseline SUP period is 36 months for systems purchased after April 2021\.  
The table below lists the primary enterprise license modules, order-specific part numbers, prerequisites, and capabilities:

| Module Identifier / Part Number | License Category | Platform Prerequisite | Operational Capabilities and Features |
| :---- | :---- | :---- | :---- |
| **Enterprise-VSS-Base/0Ch** | VSS Base | None | Base license with 0 camera channels; includes third-party map integration, virtual machine support, and a support plan. |
| **Enterprise-VSS-Base/16Ch** (400101054) | VSS Base | None | Base license with 16 camera channels; includes alarm mapping, evidence tools, and 3-year SUP. |
| **Enterprise-VSS-Base/64Ch** (400101076) | VSS Base | None | Base license with 64 camera channels; includes main/auxiliary storage and 3-year SUP. |
| **Enterprise-VSS-Base/300Ch** (400101077) | VSS Base | None | Base license with 300 camera channels; includes health monitoring and 3-year SUP. |
| **Enterprise-VSS-1Ch** (400101055) | VSS Add-on | VSS Base | Adds 1 camera channel (standard IP camera or recorder interface). |
| **Enterprise-HPC-1Camera** | VSS Add-on | VSS Base | Integrates 1 remote camera imported through an authorized peer connection. |
| **Enterprise-BWC-1Ch** | Mobile Add-on | VSS Base | Integrates 1 body-worn camera channel with GPS and telemetry logging. |
| **Enterprise-VSS-1Ch/Thermal\&Report** | Special Add-on | VSS Base | Integrates 1 thermal imaging channel with temperature tracking and anomaly reports. |
| **Enterprise-IPSpeaker-1Unit** | Audio Add-on | VSS Base | Integrates 1 network IP speaker unit for live announcements and scheduled broadcasting. |
| **Enterprise-ThirdPartyVSS-1Ch** (401000631) | 3rd Party Add-on | VSS Base | Integrates 1 third-party Alternate Vendor IP camera using standard integration protocols. |
| **Enterprise-ACS-Base/2Door** (401000018) | ACS Base | None | Base license supporting 2 physical doors and core access control management. |
| **Enterprise-ACS-Base/16Door** (401000040) | ACS Base | None | Base license supporting 16 physical doors and core access control management. |
| **Enterprise-ACS-1Door** | ACS Add-on | ACS Base | Adds 1 access point door or 1 door station reader to the system. |
| **Enterprise-VideoIntercom-Module** (401000171) | Intercom Base | VSS or ACS Base | Manages 2 door stations, 2 video camera channels, and 10 indoor stations. |
| **Enterprise-IndoorStation-1Unit** (401000172) | Intercom Add-on | Intercom Base | Adds 1 additional indoor intercom unit to the system. |
| **Enterprise-Visitor-Module** (401000131) | Special Module | ACS Base | Enables visitor registration, badge printing, and access credential tracking. |
| **Enterprise-Attendance-Module** (401000023) | Special Module | ACS Base | Enables shift scheduling, compliance tracking, and third-party database sync. |
| **Enterprise-Mustering-Module** (401000373) | Special Module | ACS Base | Enables automated emergency fire lock-release routing and muster checklists. |
| **Enterprise-RSM-Base/2Site** (401000003) | RSM Base | None | Core interface managing 2 remote server sites; expandable up to 1024 sites. |
| **Enterprise-RSM-1Site** (401000004) | RSM Add-on | RSM Base | Adds 1 remote server site to the master Central Management System interface. |
| **Enterprise-MS-Base** (401000428) | Mobile Base | None | Base license supporting 10 mobile units, GIS maps, and route tracking. |
| **Enterprise-MS-1Unit** | Mobile Add-on | Mobile Base | Adds 1 mobile recorder or vehicular DVR unit to the fleet management map. |
| **Enterprise-MS-Route\&Driver-Module** | Mobile Add-on | Mobile Base | Enables route planning, vehicle speed alerts, and driver behavior analysis. |
| **Enterprise-MS-PassengerCounting-Module** | Mobile Add-on | Mobile Base | Enables passenger counting using overhead stereo analysis cameras. |
| **Enterprise-DigitalSignage-Base** (401000461) | Signage Base | None | Base license managing up to 30 display monitors or interactive touch screens. |
| **Enterprise-SmartWall-Module** | Display Module | VSS Base | Enables layout management for multiple video walls and physical decoders. |
| **Enterprise-SmartWall-1Output** (401000347) | Display Add-on | Smart Wall | Adds 1 hardware decoding output link to the multi-screen system. |
| **Enterprise-SmartWall-Module Base** (401000085) | Display Add-on | Smart Wall | Allocates auxiliary graphics card resources to render alarm feeds directly. |
| **Enterprise-ANPR-Module** | LPR Module | VSS Base | Activates central parking, payment calculations, and vehicle searches. |
| **Enterprise-ANPR-1Ch** (401000056) | LPR Add-on | ANPR Module | Integrates 1 license plate recognition channel and list matching rules. |
| **Enterprise-FacialReco-Module** | Face Module | VSS Base | Enables central matching, attribute sorting, and demographic data analytics. |
| **Enterprise-Facial\&Body-1Ch** | Face Add-on | Face Module | Adds 1 camera channel for face comparison and body attribute matching. |
| **Enterprise-FacialReco-1Ch** (401000057) | Face Add-on | Face Module | Integrates 1 facial recognition channel for intelligent camera or server feeds. |
| **Enterprise-BI Report** (401000038) | BI Module | VSS Base | Enables retail heat mapping, crowd flow analyses, and queue reports. |
| **Enterprise-DEM/Module** | Evidence Module | VSS Base | Enables secure incident archiving, tag generation, and file locking. |
| **Enterprise-AR-Module** (401000581) | AR Module | VSS Base | Enables high-point map overlays and visual tag interactions. |
| **Enterprise-AR-1Scene** (401000582) | AR Add-on | AR Module | Integrates 1 high-point camera scene with adjacent camera and gate tags. |
| **Enterprise-Temp\&Mask-Module** (401000269) | Heat Module | VSS or ACS Base | Logs skin-surface temperatures, registers mask violations, and tracks trends. |
| **Enterprise-SecurityInspection-Module** (401000384) | Special Module | VSS Base | Unifies item counts, X-ray scanner imagery, and metal detector telemetry. |
| *Enterprise-SUP-Package/1Y* (401000563) | Maintenance | Existing License | Extends access to technical support and new software features for 1 year. |
| **Enterprise-SUP-Package/3Y** (401000564) | Maintenance | Existing License | Extends access to technical support and new software features for 3 years. |
| **Enterprise-Unified-Bundle** | Promotional Base | None | Unified software bundle activating 32 VSS channels and 4 ACS doors. |
| **Enterprise-VSS16ACS8-Bundle** | Promotional Base | None | Unified software bundle activating 16 VSS channels and 8 ACS doors. |
| **Enterprise-VSS32ACS16-Bundle** | Promotional Base | None | Unified software bundle activating 32 VSS channels and 16 ACS doors. |

## **Cloud-Native Solutions: cloud-managed team mode and enterprise VMS Connect**

The shift toward serverless security deployment has led to the adoption of cloud-native platforms, eliminating the need to maintain on-premises server infrastructure. Reference Vendor provides this capability through cloud-managed team mode and the enterprise VMS Connect video security as a service (VSaaS) platform.

### **VSaaS Architecture and Interactive Toolsets**

cloud-managed team mode is designed for multi-site and multi-user environments such as franchise retail networks, commercial offices, and regional operations. The cloud architecture enables centralized management without local VPN configurations or public IP mapping.  
The toolsets integrated within the Team Mode interface include:

* **Geographical Information System (GIS) Mapping:** Automatically maps distributed physical endpoints to absolute GPS coordinates, displaying global site and camera locations on an interactive overlay.  
* **Auxiliary Multi-Screen Monitoring:** Allows operators to open up to three auxiliary web browser displays to view high-density camera layouts simultaneously.  
* **semantic video search Intelligent Video Retrieval:** An AI-powered search tool that indexes metadata on the cloud, allowing operators to locate specific events using voice descriptions, text search, or target classification (human or vehicle).  
* **Unified Access Control Systems:** Enables physical doors to be unlocked remotely via the mobile app using face profiles, Bluetooth, dynamic QR codes, PINs, or NFC credentials. The system supports offline operations, maintaining credential databases on local edge readers and caching audit trails until the connection is restored.  
* **GPS Mobile Attendance Tracking:** Employees can clock in and out remotely via the mobile application, matching read requests with GPS geofences. It links attendance logs with access control permissions and adjacent video feeds to verify check-ins.  
* **Vehicular Access Management:** Automates gate barriers by connecting with ANPR edge cameras. Operators can adjust vehicle allowlists and blocklists, program remote barrier actions, and issue temporary passes directly from the cloud portal.  
* **Unified Voice & Alert Broadcasting:** Integrates intercom and alarm audio, supporting direct door-to-app two-way calling, mass alert broadcasts, and event-driven voice warnings to deter trespassers.

### **Paid Subscription Benefits and Cloud Storage Policies**

The cloud subscription model allows enterprises to scale their systems on an operational expense (OpEx) basis, paying only for the active services they require. This model avoids the initial capital expenditure of on-premises storage servers and dedicated server rooms.  
Subscription features include:

* **No Local Server Requirements:** Eliminates costs related to localized streaming servers, video storage servers, cooling, and network maintenance.  
* **Proactive Status Monitoring:** Third-party technicians can remotely monitor system health and perform configurations through the platform.  
* **Free Deployment Tiers:** The system is free to configure for up to 32 video channels and 10 access control doors, allowing organizations to run proof-of-concept tests before committing to enterprise subscriptions.  
* **Flexible Licensing Structure:** Businesses can opt for monthly or yearly billing cycles, matching operations to seasonal demand or temporary project timelines.  
* **Encrypted Cloud Storage Solutions:** Replaces or complements on-site recording by saving event-triggered video or continuous loops directly to secured, encrypted cloud databases. Standard subscriptions include 7-day or 30-day loop recording options.

## **Managed Integrator Services**

A managed integrator portal can serve system integrators, distributors, and service providers by combining diagnostic utilities, network tools, project quotation management, and warranty tracking within a unified interface.

### **Service Distribution Model and Paid Value-Added Add-ons**

Such portals may use a freemium model. Installers can manage customer locations, perform network configuration audits, and upgrade firmware on a local network, while remote maintenance and monitoring may require subscription licenses.  
The table below details the split between free foundational capabilities and paid, value-added services:

| Operational Module | Functional Classification | Specific Toolsets, Features, and Capabilities |
| :---- | :---- | :---- |
| **Site Authorization Management** | Free Baseline | Create new physical customer sites, apply for site authorizations, and share site access with third-party technicians. |
| **Device Configuration Tools** | Free Baseline | Add devices via LAN autodetect, import profiles, adjust DDNS, and modify network configurations remotely. |
| **Remote Support Diagnostics** | Free Baseline | Reset device passwords (onsite or offsite), retrieve system logs remotely, and push batch firmware upgrades to LAN devices. |
| **Alarm Receiving Center (ARC)** | Free Baseline | Connect customer alarm control panels with standards-based security receivers. |
| **Commercial Operations Tools** | Free Baseline | Access product spec selectors, coordinate project registrations, calculate pricing models, and track RMA logistics. |
| **Health Monitoring Service** | Paid Subscription | Automates system health checks, tracks real-time status, identifies exceptions, and triggers automated linkage rules. |
| **Cloud Storage Service** | Paid Subscription | Activates direct-to-cloud backups for video feeds, supporting standard IP cameras, solar-powered units, and NVRs. |
| **Cloud Attendance Service** | Paid Subscription | Deploys cloud-hosted personnel shift tracking, vacation approvals, and attendance reports without local software installations. |
| **Premium ARC Gateways** | Paid Subscription | Enables advanced ARC configurations, matching video verification streams with automated dispatch protocols. |
| **Cellular IoT Data Service** | Paid Subscription | Activates and manages global 4G data cards used in remote solar-powered cameras or vehicular tracking units. |
| **Temperature Screening Service** | Paid Subscription | Enables advanced skin-temperature trend reports, mask screening metadata audits, and integration with local access policies. |
| **Employee Sub-Account Add-On** | Paid Subscription | Enables granular role-based permissions and activity tracking for secondary technician accounts. |

### **Global Regulatory and Regional Availability Framework**

Due to differences in telecommunications infrastructure, regional data privacy laws, and localized distributor agreements, subscription-based features are subject to strict regional availability limitations:

* **United States and Canada:** Installers can purchase and configure Health Monitoring, Cloud Storage, and VSaaS modules. However, due to local regulatory constraints, the configuration of automated Linkage Rules within the Health Monitoring Service is restricted.  
* **Africa:** Integrators can access and use the platform's free features, including site management, firmware updates, and basic device configurations. However, paid value-added services are not supported across multiple countries, including Angola, Benin, Botswana, Burkina Faso, Burundi, Cameroon, Cape Verde, Central African Republic, Chad, Comoros, Congo, Cote D'Ivoire, Djibouti, Equatorial Guinea, Eritrea, Ethiopia, Gabon, Gambia, Guinea, Guinea-Bissau, Liberia, Madagascar, Malawi, Mali, Mayotte, Mozambique, Namibia, Niger, Nigeria, Rwanda, Senegal, Seychelles, Sierra Leone, Somalia, Tanzania, Togo, Uganda, Zambia, and Zimbabwe.  
* **Asia (Japan and Taiwan):** Support is limited strictly to the platform's free feature set, with paid cloud services and premium subscriptions unavailable.  
* **Europe and Latin America:** Serves as the standard deployment model for cloud integrations. Integrators have unrestricted access to purchase, configure, and renew all value-added cloud subscriptions, health monitoring tools, and cellular data plans.

## **Analytical Synthesis and Deployment Recommendations**

Deploying an optimal security software architecture requires aligning physical scale, integration complexity, and financial models with the capabilities of the chosen platform:

* **Deploying Local Utility Software (desktop VMS client):** This architecture is best suited for small, single-site properties where security operations remain within a local network. It is ideal for budgets that prioritize avoiding recurring licensing fees and where on-site workstations can handle the software's decoding demands. However, as device counts approach database limits, performance can degrade, making this model less suitable for larger, expanding systems.  
* **Deploying On-Premises Central Management (enterprise VMS platform):** Recommended for complex, high-density environments such as industrial plants, transportation hubs, corporate campuses, and hospitals. This model is ideal for organisations that require deep integration between video feeds, access control doors, vehicle barriers, thermal monitors, and local video walls. While it requires significant initial capital expenditure for dedicated server hardware and licensing, it ensures data sovereignty, low-latency processing, and high-performance operation on-site.  
* **Deploying Cloud-Native VSaaS (cloud-managed team mode & enterprise VMS Connect):** Well-suited for geographically distributed, multi-location businesses such as retail chains, property management groups, and temporary construction projects. By moving core management systems to the cloud, it avoids the costs of on-site IT rooms, server maintenance, and complex VPN configurations. This model allows organizations to transition from capital-heavy hardware deployments to predictable, flexible monthly or yearly operational subscription fees.

#### **Works cited**

The original vendor-specific source list was removed for public distribution. Any
future compatibility claims must be backed by independently reviewed, redistributable
documentation and recorded in the project evidence log.
