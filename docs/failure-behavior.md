# P0-03 Failure-Behavior Contract

> **Document role:** Normative technical detail for the approved P0-03 contract. Approval status, implementation evidence, later feature order, and verification checks live only in [`../Development_plan.md`](../Development_plan.md). This document does not authorize any later-gate service.

## 1. Purpose and status

### A. Purpose

This document defines the required user-visible behavior of the VMS when an
application, device, network path, data store, optional service, or local
resource is unavailable.

The governing rule is:

> A failure may reduce VMS functionality, but the VMS must not claim that a
> device is offline, recording has stopped or continued, an action succeeded,
> data was saved, or synchronization completed unless it has evidence for that
> exact claim.

### B. Normative language

The terms `MUST`, `MUST NOT`, `SHOULD`, `SHOULD NOT`, and `MAY` describe the
P0-03 product contract:

- `MUST` and `MUST NOT` are required invariants.
- `SHOULD` and `SHOULD NOT` are the default behavior. A later gate may approve
  a documented exception.
- `MAY` describes an optional capability that must advertise whether it is
  available.

### C. Implementation status

This is a behavioral and architectural contract. It is not evidence that the
current prototype already contains:

- a production background collector or site agent;
- a coordinator, cluster, replicated database, queue, or failover service;
- real vendor-specific, ONVIF, cloud, access-control, or media operations;
- durable alarm or audit reconciliation;
- production authentication or offline authorization;
- VMS-owned continuous recording.

Those capabilities remain unavailable until their controlling roadmap items
are approved, implemented, and verified. Implementing policy types, state
calculations, and replaceable boundaries under P0-03 must not be represented as
implementing the services behind those boundaries.

## 2. Product boundary

### A. Optional data and interaction layer

The VMS is an optional data-acquisition, normalization, manipulation,
presentation, and interaction layer. It can consume supported information from
cameras, NVRs, DVRs, storage devices, controllers, vendor services, open
protocols, and future integrations.

The VMS is not required for an existing camera, NVR, or DVR to perform its
native functions. Stopping, closing, restarting, or losing this VMS MUST NOT by
itself stop native device recording, schedules, local access rules, local
alarms, or other autonomous device behavior.

When a later approved feature intentionally sends a device-changing command,
that command can affect the target device by design. Such an effect must result
from an explicit approved workflow, not from a VMS outage or recovery side
effect.

### B. Recorder-owned footage

For the current product direction:

- Cameras, NVRs, DVRs, device storage, or user-selected external storage remain
  authoritative for their recordings.
- Registering a device MUST NOT open a permanent media stream.
- The VMS opens live, playback, search, or download sessions only when an
  approved foreground or background function requires them.
- A loss of VMS communication means recording state may be unknown; it does not
  prove recording stopped.
- The VMS MUST NOT claim recording continuity for an interval it could not
  verify.

### C. Future VMS recording

Using the VMS as an NVR or recording server, including local, remote, replicated,
or distributed footage storage, is deferred. Its recording ownership,
retention, failover, gap detection, reconciliation, and recovery contract must
be approved under the recording and storage roadmap gates before use.

Adding that capability later MUST use a replaceable recording provider. It
must not change the present rule that independently configured cameras, NVRs,
and DVRs remain autonomous.

## 3. General failure principles

### A. Internal failure isolation

Failure isolation applies to the VMS itself. The implementation MUST prevent an
unrelated VMS workload from failing merely because:

- one site or network path is unavailable;
- one recorder or camera times out;
- one adapter returns malformed, unsupported, or unavailable data;
- one live-view tile fails;
- one import, polling, playback, search, or download job fails;
- one optional cloud or vendor service is unavailable.

Isolation does not mean the VMS keeps physical devices operational. Those
devices are autonomous. It means one failed VMS dependency must not
unnecessarily freeze, exhaust, or falsify unrelated VMS work.

### B. Graceful degradation

The VMS MUST:

- keep functions working when all of their actual dependencies are available;
- disable only the functions whose dependencies are unavailable;
- state the failed dependency and the last confirmed observation where known;
- preserve cached data as explicitly stale rather than present it as current;
- reject writes that cannot be durably committed;
- avoid hidden loss, silent fallback, and false success;
- keep connectivity, capability, functional health, recording, and storage
  conclusions separate.

### C. Capability-dependent behavior

A behavior that depends on optional device history, edge storage, byte-range
download, stable event identifiers, checksums, or another capability MUST be
enabled only after that capability is positively discovered or configured and
verified for the selected adapter and device.

The VMS MUST NOT silently substitute:

- an unverified protocol for a failed adapter;
- an undocumented vendor endpoint for a failed standard endpoint;
- a cloud path for a local path, or a local path for a cloud path, without an
  approved connection policy;
- a guessed status for an unsupported observation.

## 4. Availability matrix

### A. Reading the matrix

The matrix defines the required result once the named function exists. `Local`
means the function and its dependencies are reachable within the installation
or site. `Cached` means a consistent last-known read model with a visible
observation or synchronization time.

Native device recording in this matrix refers only to recording already
configured on a camera, NVR, or DVR. It is not VMS-owned recording.

| Failure condition | Inventory and last-known data | Current observations and alarms | Live view | Playback, search, and download | Native recording | VMS writes and controls |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Operator UI closes or crashes | Available after restart from the configured VMS data source | Continue only if a verified background continuity provider is running; otherwise become stale | UI sessions end; abandoned media resources are released | Active UI work ends; resumability is capability-dependent | Unaffected by the UI | Unavailable until an authorized UI/client returns |
| Local VMS backend or collector stops | Cached data may remain readable through another available component | No collection through the failed component; mark freshness stale/unknown | Available only through another healthy approved media path | Available only through another healthy approved source path | Unaffected by the VMS component | No action may report success through the failed component |
| Operator-to-device or site LAN path fails | Cached, read-only, visibly stale | Affected observations become unknown/stale; raise the proven path/root cause instead of thousands of false device failures | Unavailable through the failed path | Unavailable through the failed path; local indexed metadata may remain searchable as stale | Continues only if the camera-to-recorder/storage path and device configuration still support it; otherwise unknown | Unavailable through the failed path |
| Camera-to-recorder path fails while the recorder remains reachable | Camera and recorder inventory remain available | Camera/channel path failure may be reported when confirmed | Direct camera live may still work through another verified path; recorder-proxied live may fail | Previously recorded footage may remain available; the affected new interval may contain a gap | Affected channel recording may stop, use verified edge recovery, or be unknown; do not assume | Unrelated recorder/channel operations remain available |
| WAN or internet fails while the site LAN remains healthy | Local authoritative or cached inventory remains available; central data becomes stale | Local collection continues only through an available continuity provider; central delivery pauses | Local direct view may continue; remote/cross-site view is unavailable without another valid path | Local source operations may continue; remote/cloud-dependent operations pause | Unaffected when native recording paths are local and healthy | Local safe operations may continue under approved authorization; shared/central changes become read-only |
| Optional coordinator or central API fails | Standalone installations are unaffected; coordinated sites use a consistent last-known read model | Local collection may continue; central dashboards and acknowledgements become stale or unavailable | Local view may continue when authorization and media paths remain valid | Local source operations may continue; central-only search/routing is unavailable | Unaffected | Shared configuration, users, roles, credentials, and cross-site changes are blocked |
| Authoritative VMS database fails | Consistent cached read model may remain available and must be marked read-only | Continue only through an independently available durable continuity provider | May continue through a valid direct path and approved cached authorization | May continue where the source and authorization are independent of the failed store | Unaffected | All writes requiring the failed authority are rejected; nothing is reported saved |
| Recorder becomes unreachable | Recorder and child inventory remain cached | Recorder reachability may be `Unreachable`; dependent channel, disk, and recording observations become `Unknown` unless independently verified | Recorder-sourced live is unavailable; an independently configured direct-camera path may remain available | Recorder-owned footage operations are unavailable | Current recorder recording state is unknown, not automatically stopped | Recorder commands are unavailable; unrelated recorders continue |
| One camera or channel fails while the recorder is healthy | Inventory remains available | Only the proven camera/channel condition is affected | Affected view fails; other channels continue | Existing recorder footage may remain available | Only the affected channel is implicated | Other cameras and recorder functions continue |
| Recorder disk or storage fails while media service remains reachable | Inventory remains available | Storage failure and recording state are reported separately | Live may remain available | Playback/download availability depends on the affected storage and requested interval | Recording may be stopped, degraded, rerouted, or unknown according to evidence | Unrelated safe functions continue |
| Optional cloud or vendor service fails | Local/self-hosted inventory remains available; cloud-only data becomes stale | Only observations obtained exclusively from that service are unavailable | Direct local paths continue; service-routed view fails | Service-owned or service-routed footage operations fail or pause | Local native recording is unaffected; cloud-owned recording state follows verified evidence | Only service-dependent writes are blocked; no undocumented fallback |
| Optional licensed component becomes unavailable | Core inventory remains accessible | Only component-specific observations are unavailable | Core/direct media continues if it does not require that component | Evidence and metadata remain accessible; decoding may be unavailable if the missing component is required | Native recording is unaffected | Only operations requiring that licensed component are blocked |
| Audit persistence becomes unavailable | Read-only access follows deployment policy and remains visibly degraded | A critical audit failure is raised through any available alternate path | Secure default permits view-only operation with a warning | Secure default permits view-only operation with a warning | Native autonomous recording remains unaffected | Secure default blocks privileged, configuration-changing, physical, and destructive actions |
| CPU, memory, decoder, connection, bandwidth, queue, or disk pressure is reached | Inventory remains available | Capacity reserved for alarm intake, audit persistence, and health/root-cause reporting | Existing approved high-value views are protected before background work; new work may be downgraded or rejected clearly | Low-priority search/download/preload work may pause or be rejected | Native recording is unaffected | Interactive safe work uses bounded admission; no unlimited queue |

### B. Background continuity

The VMS MUST NOT promise continuous collection merely because a device is
registered.

Continuous VMS-side status, alarm, or audit collection while the UI, WAN, or
coordinator is unavailable requires a verified source-near continuity provider.
Depending on later approved architecture and device capabilities, that provider
MAY be:

- the local VMS backend running as a background service;
- a site collector or worker;
- verified durable recorder/device event history that can be reconciled later;
- a future redundant combination of these.

The UI MUST show whether continuous collection is active, degraded, or not
configured. A missing provider affects VMS observation continuity only; it does
not make the physical devices dependent on the VMS.

The exact service layout, worker ownership, queue, heartbeat, election, and
failover mechanism are deferred.

## 5. Inventory and configuration behavior

### A. Cached reads

When the authoritative data source is unavailable, a consistent cached read
model MAY be used. Cached data MUST include or be accompanied by:

- the time it was last successfully synchronized or observed;
- the authority or source from which it was obtained;
- an explicit read-only and stale indication;
- enough version information to prevent partial or mixed configuration from
  being presented as one committed state.

The exact snapshot format, database, atomic replacement mechanism, migration,
and conflict rules belong to P0-04 and P1-10.

### B. Writes

When the required authority is unavailable, the VMS MUST NOT pretend to save,
queue, or synchronize:

- device additions, edits, deletions, moves, or group changes;
- user, role, permission, credential, or session-policy changes;
- persistent device configuration;
- shared layouts, rules, or site configuration.

The P0-03 default is read-only shared configuration during authority loss. A
later synchronization gate may approve a narrowly defined offline-write type
only when it has version checks, conflict behavior, durable audit, and a safe,
idempotent reconciliation contract.

## 6. Media-session and transfer behavior

### A. Live view

Live view is available only when the selected source, path, adapter capability,
authorization, and resource budget are available.

One failed tile MUST NOT freeze the grid. Hidden and off-screen tiles MUST NOT
retain media sessions merely because they were previously visible. Exact
main-stream, sub-stream, tile, decoder, and reconnect policies remain assigned
to the media and capacity gates.

### B. Playback and search

Recorder-side playback or search requires the recorder or authoritative
recording source to be reachable. Locally indexed metadata may remain searchable
when its source is unavailable, but results MUST show their freshness and MUST
NOT imply that the underlying footage is currently retrievable.

Missing or unverified intervals MUST remain visible as gaps or unknown periods.

### C. Interrupted downloads

An interrupted export or download MAY resume only when the source supports a
verified resume mechanism, such as a stable vendor export identifier or a
strong representation validator with byte ranges.

If source identity cannot be verified, the transfer MUST restart rather than
append blindly. A local hash proves the integrity of the resulting local bytes
after hashing; it proves equality with the source only when the source supplies
a trusted matching digest or equivalent verified mechanism. Exact evidence,
chain-of-custody, and export-integrity rules belong to P5-08 and P5-09.

## 7. Device commands and physical access

### A. Command classes

Every future adapter operation must be classified before retry policy is
applied:

- observation or safe read;
- idempotent desired-state write;
- momentary action;
- physical action;
- destructive or security-sensitive action.

Momentary, physical, destructive, and security-sensitive actions include, when
later approved, unlock, barrier movement, relay or alarm output, broadcast,
PTZ movement, reboot, shutdown, format, delete, password change, firmware, and
similar operations.

### B. No delayed replay

Momentary, physical, destructive, and security-sensitive actions:

- MUST have a unique command ID, actor, target, issue time, and expiry;
- MUST NOT be stored for automatic execution after their original context or
  connection has expired;
- MUST NOT be automatically replayed after an uncertain outcome;
- MUST show `Outcome unknown` if transmission may have occurred but
  confirmation was not received;
- require a refreshed target state and a new authorized operator decision
  before another attempt.

An idempotent desired-state write may be retried only when its later feature
gate proves version checking, authorization, auditability, and idempotence.

### C. Access decisions

Future access-control support MUST preserve controller autonomy:

- Locally committed credentials, schedules, emergency behavior, and
  life-safety rules remain controller responsibilities.
- The VMS MUST NOT synthesize an authorization grant after an external
  authorization request has timed out.
- The VMS MUST NOT replay a late unlock or grant after reconnection.
- The controller follows its approved local offline, timeout, egress, fire,
  and life-safety policy.
- WAN failure is not the same as power failure.
- Fail-safe and fail-secure behavior is a per-door commissioning and safety
  decision, never one global VMS default.

open interoperability defines an external-authorization timeout event but does not prescribe
one universal controller fallback decision. Exact authorization duration,
cached credentials, revocation limits, door behavior, and safety approval
remain P0-16 and P8 decisions.

## 8. Alarm and event continuity

### A. Durable acceptance

Where continuous alarm collection is approved, a continuity provider must
durably accept an event before that provider treats ingestion as complete. If
no durable provider is available, the VMS MUST show that continuous collection
is unavailable and MUST NOT promise later recovery.

When supplied by the origin, the VMS SHOULD preserve:

- origin source and event ID;
- origin sequence;
- occurrence time;
- receipt time;
- synchronization time;
- type, severity, site, device, and channel identity.

Time interpretation and ordering remain subject to P1-08.

### B. Honest event identity

The normalized event model must distinguish:

- `originEventId`: a stable identifier supplied by the originating device or
  service, when available;
- `ingestionEventId`: an immutable identifier assigned at first durable VMS
  ingestion;
- delivery or correlation identifiers used for later processing.

Central reconciliation may safely deduplicate the same ingested record by its
ingestion ID, or a repeated origin event by a verified origin source and origin
ID.

When a device supplies no stable event ID or sequence, a newly assigned VMS ID
cannot prove that two similar device responses describe the same occurrence.
The VMS MUST NOT silently merge such records using time-and-payload guesses.
Possible duplicates, missing history, and unverifiable intervals must remain
representable.

### C. Recovery

Recovered events must retain their original occurrence time when supplied and
must be visibly marked as delayed when delivery was delayed. If device history
is unavailable or incomplete, the VMS must expose a history gap rather than
claim complete reconciliation.

Alarm acknowledgement and alarm clearance are separate facts. An
acknowledgement records an actor and time; it does not prove that the underlying
condition cleared.

Exact ingestion throughput, queue, ordering, acknowledgement, deduplication,
retention, and alarm lifecycle behavior belong to P6-04 and P6-07.

## 9. Audit behavior

### A. Secure default

The default P0-03 policy when durable audit persistence is unavailable is:

- raise a critical audit-system failure through any available alternate path;
- allow read-only inventory, live view, and playback with a prominent warning;
- continue autonomous camera, recorder, controller, egress, and life-safety
  behavior;
- block VMS privileged configuration, physical actions, destructive actions,
  and other operations whose required audit record cannot be durably accepted;
- never silently overwrite audit records to hide capacity exhaustion.

Where an audited action is supported, the VMS should durably record intent
before dispatch and the confirmed or unknown outcome afterward. It MUST NOT
report a confirmed successful outcome until the required success audit record
is durably accepted.

### B. Configurable policy

Audit-failure response is an organization-defined deployment policy, not one
universal behavior required by NIST. A later approved security policy MAY be
stricter, such as blocking view operations or entering a broader degraded mode,
or MAY use an approved alternate audit sink.

The secure default must therefore be implemented behind a replaceable policy
boundary. No deployment policy may use audit failure to disable required local
egress or life-safety behavior.

Exact immutable storage, capacity, integrity protection, retention, trusted
timestamps, alternate sinks, and synchronization belong to P0-16 and P1-06.

## 10. Multidimensional health model

### A. Separate dimensions

The VMS MUST NOT reduce all device information to one `online/offline` value.
At minimum, the normalized health model must keep these dimensions separate:

| Dimension | Minimum normalized values | Meaning |
| :--- | :--- | :--- |
| Reachability | `reachable`, `unreachable`, `unknown`, `not-applicable` | Whether a recent valid connectivity test proved a path to the target |
| Freshness | `fresh`, `stale`, `never-observed` | Whether the observation is current enough for its policy; includes observation time |
| Functional health | `healthy`, `degraded`, `failed`, `maintenance`, `recovering`, `unknown` | Operational condition reported or inferred from valid evidence |
| Recording | `recording`, `not-recording`, `unknown`, `not-applicable` | Recording state for the exact source/channel and interval |
| Storage | `healthy`, `warning`, `failed`, `unknown`, `not-applicable` | Storage condition independently of live connectivity |
| Capability | `supported`, `unsupported`, `unavailable`, `unknown` | Whether the selected adapter/device can perform or observe a function |

Exact thresholds, observation windows, reason codes, and operator labels remain
P1-09 and P2-13 decisions.

### B. Composite UI labels

The UI may calculate a concise label from the separate dimensions, but it must
not discard the source facts.

- `Online` requires a fresh, valid reachability observation.
- A stale previous result is shown as `Last known online - current status
  unknown`, not as current `Online`.
- A timeout or proven path failure may produce `Unreachable`.
- A syntactically or semantically invalid response is an adapter/protocol or
  functional error; it does not by itself prove network unreachability.
- Unsupported or unimplemented capability is not a device failure.
- Loss of a parent recorder or site path makes dependent child observations
  unknown unless an independent path verifies them.

### C. Root-cause presentation

When one site path, collector, recorder, or shared dependency explains many
unverified child devices, the VMS SHOULD raise one primary root-cause condition
and associate the affected resources. It MUST NOT manufacture thousands of
independent camera-offline alarms without independent evidence.

## 11. Resource pressure and admission control

### A. Registered versus active capacity

Registered inventory capacity and simultaneously active workload capacity are
different measurements. A large inventory MUST NOT create continuous media
sessions or aggressive polling for every registered device.

### B. Priority behavior

The approved direction separates a system-critical lane from configurable
`high`, `medium`, `low`, and `idle` work:

- System-critical capacity is reserved for alarm intake, durable audit,
  coordination health, and recovery control.
- `high` is active operator viewing or incident work.
- `medium` is requested playback, search, download, or important health work.
- `low` is thumbnail, discovery, capability refresh, or background metadata
  work.
- `idle` retains inventory without an active media stream and uses only
  appropriately staggered checks.

Under pressure, the default degradation order is to pause idle work, slow low
work, stop optional thumbnails or discovery, pause or reschedule eligible
downloads, reduce eligible media quality, and finally reject new work clearly.
Alarm and audit capacity must not be consumed by unlimited media or download
queues.

Exact budgets, fairness, aging, hysteresis, automatic optimization, manual
overrides, grid limits, stream selection, and hardware thresholds remain later
capacity and media decisions.

## 12. Retry, reconnection, and reconciliation

### A. Retry ownership

The VMS must use shared retry-policy contracts and bounded budgets. It MUST NOT
depend on one literal global scheduler that would become a bottleneck or single
point of failure.

Retry execution MAY be distributed by site, adapter, worker, or dependency,
provided that:

- one architectural layer owns retries for a given call chain;
- nested layers do not multiply retries;
- each dependency and site has bounded concurrency and retry budgets;
- recovery work cannot starve foreground operator or system-critical work.

### B. Retry classification

Temporary network errors, timeouts, throttling, and temporary service errors
may be retried using capped exponential backoff, jitter, and an attempt or
elapsed-time limit.

Invalid credentials, authorization denial, unsupported operations, invalid
configuration, and expired commands MUST NOT loop until their condition
changes. Non-idempotent, physical, destructive, and outcome-unknown actions
MUST NOT be automatically retried.

### C. Recovery order

The default recovery order is:

1. site control and authorized dependency paths;
2. alarm and audit continuity;
3. recorder, camera, storage, and capability observations;
4. operator-selected media;
5. approved playback and download work;
6. medium, low, and idle background work.

The implementation must stagger recovery to avoid a reconnect storm. It must
recalculate current health, synchronize only duplicate-safe records, and expose
outage duration, remaining work, and known or unknown event/recording gaps.

Exact intervals, randomization algorithm, concurrency values, circuit-breaker
thresholds, and recovery SLOs remain capacity and resilience-test decisions.

## 13. Modular implementation boundaries

### A. Required separation

P0-03 behavior must be represented through vendor-neutral, replaceable
contracts. The core must not import concrete vendor, ONVIF, database, queue,
cloud, or operating-system service logic to decide failure meaning.

The architecture should expose independently testable roles equivalent to:

- failure and availability policy;
- dependency and root-cause evaluation;
- continuity provider for observations and events;
- consistent cached-read provider;
- authoritative write provider;
- alarm spool and reconciliation provider;
- audit sink and audit-failure policy;
- authorization-continuity policy;
- retry policy and retry-budget provider;
- workload admission and priority policy;
- media resume and integrity verifier;
- device/vendor adapter.

These are logical boundaries, not mandatory class names, processes, databases,
or deployment units.

### B. Replacement safety

Each boundary MUST:

- have an explicit versioned contract or stable normalized input/output shape;
- return explicit `succeeded`, `failed`, `unsupported`, `unknown`, or
  `unavailable` outcomes as applicable;
- declare capabilities rather than rely on vendor-name checks;
- make unavailable/null implementations fail clearly without fabricated data;
- be testable without physical devices or external services;
- avoid changing unrelated public contracts when one implementation is added,
  removed, or replaced.

Policy selection must be configuration-driven and validated. A missing or
unknown policy value must use a documented conservative default, not an
implicit vendor fallback.

### C. Six standards corrections incorporated

This contract deliberately incorporates these corrections:

1. Continuous monitoring requires a verified continuity provider, not one
   hard-coded site-agent topology.
2. An external-authorization timeout never creates a late VMS grant; controller
   fallback remains an approved local policy.
3. A VMS-assigned ingestion ID does not falsely claim duplicate detection for
   source events that lack stable identity.
4. The secure audit-failure response is configurable because the standard
   leaves the response organization-defined.
5. Health is multidimensional; stale, unsupported, invalid, and unreachable are
   not collapsed into `offline`.
6. Retry behavior uses shared contracts and per-scope budgets, not one global
   scheduler or independent nested retry storms.

## 14. Roadmap cross-references

P0-03 approves failure semantics, not the following implementations:

| Deferred decision | Owning roadmap gate |
| :--- | :--- |
| Authoritative database, local/central ownership, snapshot format, replication, migration, conflicts, and reconciliation | P0-04 / P1-04 |
| Configuration backup, restore, atomic import, rollback, and version compatibility | P1-10 |
| Operating-system, codec, hardware, numeric capacity, and auto/manual optimization thresholds | P0-05 through P0-13 / P12-03 |
| Authentication, cached authorization, session duration, revocation, roles, step-up confirmation, privacy, and security baseline | P0-16 / P1-02 / P1-03 |
| Immutable audit data model, storage, retention, integrity, and synchronization | P1-06 |
| APIs, event bus, queues, collectors, site agents, workers, load balancing, and deployment topology | P1-05 / P7-03 |
| Time, time zones, device clock drift, and disconnected-event ordering | P1-08 |
| Final health states, thresholds, observability, diagnostics, maintenance, acknowledgement, and recovery labels | P1-09 / P2-13 |
| Device discovery, capabilities, adapters, network setup, remote support, and interoperability | P0-14 / P2-03 / P2-07 / P2-11 / P2-12 / P2-14 |
| Stream selection, tile/grid limits, decoding, media reconnect, and live diagnostics | P3-01 through P3-04 |
| Future VMS-owned recording, storage placement, retention, failover, cloud recording, and recording-gap recovery | P4-01 through P4-10 |
| Playback, source selection, search, download, evidence integrity, and chain of custody | P5-01 through P5-09 |
| Alarm ingestion, ordering, deduplication, rule behavior, acknowledgement, notification, and automation | P6-03 through P6-08 |
| Cloud/P2P topology, offline maps, remote-site routing, and vendor-cloud dependencies | P7-02 through P7-04 / P10 |
| Doors, offline edge access, remote unlock, fail-safe/fail-secure commissioning, and life-safety workflows | P8-03 through P8-12 |
| Open-source license choice, optional component licensing, entitlements, grace, and expiry | P0-17 / P11 |
| High-availability technology and complete LAN/WAN/cloud/restart/reconnect testing | P12-04 |
| Security, safety, evidence, backup, migration, and recovery validation | P12-05 through P12-07 |

No availability, capacity, security, evidence, or high-availability claim is
complete until the relevant later gate is approved and its behavior is tested.

## 15. Standards basis

This contract uses the following standards and primary guidance without
claiming they prescribe this product's complete architecture:

- [ONVIF Profile G](https://www.onvif.org/video/ovif-profile-g-for-edge-storage-and-retrieval/)
  establishes capability-based edge recording, search, retrieval, and playback
  integration.
- [ONVIF PACS Architecture and Design Considerations](https://www.onvif.org/specs/wp/ONVIF-PACS-Architecture-and-Design-Considerations.pdf)
  describes controller-side storage and intelligence for offline operation
  while avoiding a mandatory physical layout.
- [ONVIF Access Control Service Specification](https://www.onvif.org/specs/srv/access/ONVIF-AccessControl-Service-Spec.pdf)
  defines external-authorization requests and timeout events; it does not
  establish one universal door fallback policy.
- [CloudEvents](https://github.com/cloudevents/spec/blob/main/cloudevents/spec.md)
  defines producer-controlled `source` plus `id` identity for recognizing a
  repeated event.
- [NIST SP 800-53 Rev. 5.1, AU-5](https://csrc.nist.gov/pubs/sp/800/53/r5/upd1/final)
  requires alerting and organization-defined responses to audit logging
  failures and supports alternate, partial, or degraded operation policies.
- [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html) defines HTTP
  idempotence, restrictions on automatic retry of non-idempotent requests, and
  strong validation for safe range resumption. These HTTP rules inform but do
  not govern non-HTTP device protocols.
- [AWS retry guidance](https://docs.aws.amazon.com/wellarchitected/latest/framework/rel_mitigate_interaction_failure_limit_retries.html)
  documents capped exponential backoff, jitter, and retry limits to avoid retry
  storms.
