# Camera Tracking Architecture

## Direction

Use this design. A flat `previous` / `next` list is too weak for real CCTV work because movement forks, subjects disappear between cameras, and operators often handle more than one query at the same time.

The correct model is:

- Directional camera graph
- Ranked candidate cameras
- Tracking sessions
- Breadcrumb timeline
- Pause/resume for concurrent cases
- Escalation from lost/found to theft or incident

## Directional Graph

A camera is a node. A route edge describes movement from a specific exit zone in one camera to a specific entry zone in another camera.

Example:

```text
Cam Lobby 01 exit: top-left -> Cam Corridor 03 entry: bottom-right
```

Edges are not automatically symmetric. Walking from Camera A to Camera B is not always the same as walking from Camera B to Camera A.

## Edge Fields

- `from_camera_id`
- `from_exit_zone`
- `to_camera_id`
- `to_entry_zone`
- `walk_time_seconds`
- `confidence`
- `bidirectional`
- `notes`

## Self-Building Route Graph

The operator should not have to map the whole site upfront.

When tracking mode is active and the operator manually jumps from one camera to another, the app should ask:

```text
Save this switch as a route edge?
```

That lets the route graph grow from real operator work.

## Candidate Strip

When tracking from a camera, the app should show the top 2-3 likely next cameras, not force one automatic jump.

Candidate ranking uses:

- edge confidence
- exit zone
- walk time
- recent operator choices
- later: optional re-identification hints

## Tracking Sessions

A tracking session represents the work, not just the camera.

Examples:

- lost phone
- lost wallet
- theft
- compliance observation
- customer movement trace

Session fields:

- case type
- status: active, paused, resolved
- priority: normal, urgent
- subject descriptor
- reported by
- start time
- last active time
- linked WhatsApp/message reference

## Breadcrumbs

Every camera visited during a tracking session should create a breadcrumb:

- session id
- camera id
- entered time
- exited time
- selected route edge
- operator note
- future thumbnail path

The breadcrumb list becomes the incident timeline.

## Concurrent Queries

The app should support:

- urgent active session in main view
- paused sessions in a slim side strip
- one-click resume
- one-click swap
- subject descriptor chip always visible

This solves the real control-room problem: one lost/found query can be paused while a theft-in-progress gets priority, without losing the first trace.

## Reverse Time and Area Search

For unknown camera cases, such as a lost phone:

- search by area/tag
- search by time range
- open candidate cameras in synced playback panes
- promote a found moment into a tracking session

## Escalation

A lost/found session can become a theft case without losing breadcrumbs.

Escalation should:

- change case type
- set priority urgent
- keep all breadcrumbs
- draft WhatsApp-ready incident text

## AI Later

Do not depend on AI first.

Build graph/session/breadcrumb mechanics first. Later, use a lightweight re-identification model only on the 2-3 candidate cameras in the strip, as a suggestion layer.

## Build Order

1. Add schema for camera edges, tracking sessions, and breadcrumbs.
2. Add manual edge entry and save-switch-as-edge prompt.
3. Add tracking sessions: start, pause, resume, resolve.
4. Auto-log breadcrumbs when switching cameras in a session.
5. Add ranked candidate strip.
6. Add split-attention paused-session sidebar.
7. Add reverse time/area search.
8. Add escalation to incident.
9. Add optional re-identification hints later.
