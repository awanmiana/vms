# UI Redesign Plan - VMS Operator Console

Status: Accepted as the working frontend direction only. This document does not approve product features or override the controlling roadmap gates.

Scope: Frontend shell and screen architecture first. Backend services, real media decoding, device adapters, and database wiring remain separate tracks.

Common screens must use normalized VMS concepts and later capability flags rather than vendor-name conditionals. Vendor-specific panels, when approved, belong to optional adapter extensions.

## Why This Reset

The current prototype runs, but the product direction is confused. It mixes an desktop-VMS-style dashboard, a web-admin layout, and early native-console ideas in one shell. The result feels heavier and older than the VMS we actually want.

Known issues:

- `app.js` is a large monolith with state, rendering, data access, event wiring, and workflow logic in one file.
- Live and playback screens scroll vertically instead of behaving like fixed operator canvases.
- Configuration screens have too much visual weight compared with the operational Monitor and Playback screens.
- Placeholder modules look too similar to finished modules.
- The frontend still uses `localStorage`; it is not wired to the backend service layer.
- Real video, playback bridge, open interoperability/vendor adapters, secure credentials, and SQLite runtime are not built yet.

## Design Direction

- Modern native-feeling operator console, closer to a quiet command/ops tool than an old DVR utility.
- Dark-neutral default theme, with light theme preserved.
- 8px maximum radius.
- Fewer decorative cards.
- Dense tables, drawers, split panes, and fixed video canvases.
- Consistent icon style.
- Right-side drawer pattern for detail/edit/PTZ/evidence.
- Command palette later for advanced actions.

## Information Architecture

Primary modules:

- Monitor
- Playback
- Events
- Devices
- Map
- Reports
- Access
- Settings

Operational modules should come first. Configuration should not be mixed into Monitor or Playback.

## Screen Plan

### Monitor

Replaces the old Operator naming.

- Left: camera/group/resource tree.
- Center: live video stage using the existing spatial canvas.
- Toolbar: grid division, stream policy, snapshot, fullscreen, PTZ.
- Right drawer: selected camera details, PTZ controls, related cameras, recent events.
- Add a clean per-feed digital zoom model separate from canvas pan/zoom.
- No page scroll; only side panels scroll internally.

### Playback

- Left: camera/group selector shared with Monitor.
- Center: playback grid, eventually migrated onto the same spatial canvas model.
- Bottom: timeline/scrubber.
- Right drawer: evidence basket, notes, export/report.
- No page scroll.

### Devices

- Table-first layout.
- Add/edit in right drawer, not a permanent form.
- Tabs: Devices, Discovery, Health, Firmware, Events.
- Credentials must move to Windows Credential Manager/DPAPI before production.

### Events

- Queue-style screen for alerts, incidents, device events, linked playback, and reporting.

### Compliance / Tickets

- Standalone module under Reports or a future Compliance module.
- Store formalized logs/tickets only.
- Show honest placeholder states until backend wiring is active.

### Settings

- Stream limits, roles, storage paths, media bridge, credentials, theme, keyboard shortcuts.

## Build Order

1. New shell: left rail, top command bar, fixed workspace, shared placeholder state.
2. Monitor rebuild.
3. Playback rebuild.
4. Devices.
5. Events/Incidents.
6. Reports/Compliance/Tickets.
7. Settings, Map, Access polish.

## Deferred

- Real RTSP/GStreamer/open interoperability media bridge.
- Frontend-to-backend API wiring.
- Real vendor adapters.
- Production authentication.
- Production credential vaulting.

## Housekeeping

- Fix `.git` metadata or initialize a real repository.
- Stop backend checks from mutating `backend/vms-dev-db.json`.
- Move frontend state behind a data-access boundary before adding more screens.

## Definition Of Done

The first redesign phase is done when Monitor and Playback are rebuilt in the new shell, both video workspaces are scroll-free, PTZ is visible in UI, and unfinished modules show consistent honest placeholder states instead of pretending to be finished.
