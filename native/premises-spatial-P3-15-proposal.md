# P3-15 premises + floor-plan/FOV slice (native increment 31)

Date: 2026-08-01  
Status: approved by the operator's directive to identify and start the next build

## Problem and user

The native spatial canvas can pan, zoom, cull, and persist camera positions, but
it has no named place beneath those positions and gives no indication of camera
direction or coverage. An administrator needs a small canonical premises model;
an operator needs to understand which floor is open and where each camera points.

## Included

- Canonical persisted `site` and `floor` entities, with one active site/floor for
  the Live workspace.
- Site name and IANA timezone; floor name, optional plan-image URI, and explicit
  world dimensions.
- A safe default `Default site / Main floor` on first run.
- Audited command-envelope verbs to configure the active site and floor.
- Persisted per-tile facing (0-359 degrees) and field of view (10-180 degrees),
  with an audited command to change them.
- A floor-plan image underlay (or an honest named blank-floor fallback), active
  premises read-out, and camera FOV wedges on the existing spatial canvas.
- Headless schema/repository, command, spatial-model, and restart round-trips.

## Explicitly excluded

- P3-18 analytics/operations fields, filters, findings, exports, or actions.
- Multi-site navigation/editor UI, buildings/wings/rooms, GIS/maps, upload/copy
  management, image calibration tools, or remote plan retrieval by the backend.
- Live decoded video inside spatial tiles (the existing honest state tiles remain).
- Camera-coordinate import, PTZ-driven direction, RBAC/tenant administration,
  or changes to the governor's viewport policy.

## Data and security boundary

The database stores plan metadata and a URI, never image bytes or credentials.
The desktop QML image loader may display the URI in the local operator session;
the persistence layer does not fetch it. Configuration uses the existing
`premises.manage` / `workspace.control` command capabilities and durable audit
path. The loopback API gains only those catalogued commands and retains its
existing bearer-authentication boundary.

## Acceptance

1. A migrated or first-run database reaches schema v11 without losing v1-v10 data.
2. Site/floor configuration and active selection survive reopen.
3. Tile facing/FOV survive workspace save/restore and appear in the QML model.
4. Spatial mode shows the active floor underlay/fallback and a direction/FOV
   marker per camera without changing decoding decisions.
5. All changes are reachable through the command envelope and its audit trail.
6. Native build, persistence/command/spatial/coverage self-checks, and offscreen
   QML smoke pass.
