# Compliance and Query Architecture

This spec extends the VMS from camera monitoring into an operator workflow for entity-level operations, location-based compliance history, and WhatsApp query resolution.

## Core Model

The application should use this hierarchy:

1. Entity: a premises or entertainment site, for example `Super Space Ocean Mall` with alias `SSOM`.
2. Location: an operational area inside that entity, for example `Bumper Car`, `XD Motion Ride`, `VR Station`, `Karaoke`, or `Cash Counter`.
3. Camera: one or more recorder channels that cover a location.

Compliance history belongs mainly to locations, not individual cameras. A single issue such as `Sale is not being punched` can be visible through multiple camera angles, and a single camera can partially cover multiple locations.

Entities and locations should also appear in the generalized tag index. The canonical tables still matter for joins and reporting, but search and filtering should treat `SSOM`, `Bumper Car`, `VR Station`, camera names, compliance types, and custom operator tags as one searchable tag surface.

## Date and Time Format

Use one strict format everywhere:

- Date: `2026-07-14`
- Time: `23:01:59`
- Combined timestamp: `2026-07-14 23:01:59`

This applies to database text fields, operator forms, WhatsApp posting formats, query ticket requests, compliance logs, playback review ranges, breadcrumbs, and UI display.

The WhatsApp posting and query formats should ask operators and managers to use this same format. That makes extraction mostly deterministic instead of dependent on fuzzy natural-language date parsing.

## Voice Command Format

The current voice system is a deliberate v1 parser:

1. Normalize the transcript: lowercase, strip filler words, convert number words, and collapse command synonyms.
2. Split compound speech into clauses on `and` or `then`.
3. Match each clause against an ordered regex rule table.
4. Resolve the spoken target through `voice_aliases`, scoped by entity type.
5. Return a `VMSCommand` with lower confidence when the target is not resolved to a known id.

This is not a full AI voice agent yet. The future agent should emit the same command shape so the executor does not care whether the command came from a click, gesture, regex voice parser, or LLM voice agent.

New alias scopes should include:

- `entity`: `SSOM`, `Super Space Ocean Mall`
- `location`: `VR Station`, `Bumper Car`
- `compliance_type`: `Staff not in uniform`, `Sale not punched`
- `ticket`: ticket numbers or short names

The tag index should mirror these concepts for search. `voice_aliases` answers "what entity did the speaker mean?" while `tag_index` answers "what records should search/filter surface for this typed tag?"

## Compliance Types

Compliance types should support both preset dropdown values and operator-entered hashtag-style values.

Examples:

- `Cashier available`
- `No cashier available`
- `Staff in uniform`
- `Staff not in uniform`
- `Sale punched`
- `Sale not punched`

Operator freeform values should create a `compliance_types` row with `created_via = 'operator_freeform'` and `review_status = 'needs_review'`. Admin review can later merge near-duplicates such as `sale not punched` and `sales not being punched`.

## Compliance Events

Compliance events are extracted facts from WhatsApp/import/manual sources. They are useful for aggregated reporting and top-three overlays.

Each event should store:

- Entity
- Location
- Compliance type
- Reported time
- Source
- WhatsApp message id when available
- Operator id when available
- Notes or message summary

## Compliance Logs

Compliance logs are the operator-confirmed record of a reviewed footage segment and the WhatsApp operational exchange around it. They are separate from raw WhatsApp messages and separate from simple aggregate events.

Each log should store:

- Entity id and entity-name snapshot
- Compliance type id and compliance-type label snapshot
- Review start timestamp, for example `2026-07-14 09:00:00`
- Review end timestamp, for example `2026-07-14 09:15:00`
- Operator who created the log
- Source WhatsApp message id for the operator's post
- Manager response WhatsApp message id
- Manager response text or summary
- Notes

Each log can link to multiple cameras and multiple locations:

- `compliance_log_cameras` stores the camera id plus camera name, device name, and channel number snapshots.
- `compliance_log_locations` stores the location id plus location name snapshot.

Snapshots are intentional. If a camera is renamed later, the historical log should still show what the operator saw and posted at the time.

The top-three compliance display should be computed from events, not stored as a duplicated camera field.

Recommended query shape:

```sql
SELECT ct.label, ct.category, COUNT(*) AS occurrences
FROM compliance_logs cl
JOIN compliance_log_locations cll ON cll.compliance_log_id = cl.id
JOIN compliance_types ct ON ct.id = cl.compliance_type_id
WHERE cll.location_id = ?
  AND cl.review_start_at >= ?
GROUP BY ct.id, ct.label, ct.category
ORDER BY occurrences DESC, ct.label ASC
LIMIT 3;
```

The UI should let the operator choose a window such as 7, 30, or 90 days. Cache the result briefly, because this is a camera overlay and should not re-query on every render.

## Camera Overlay

Camera views should be able to show:

- Stream tier and approximate bitrate
- Location tags
- Top-three compliance types for the location

The dashboard should include an on/off toggle for compliance overlays, similar to the existing media policy toggle. Later settings should include the history window: 7, 30, or 90 days.

## WhatsApp Query Tickets

Manager questions posted in WhatsApp should become tickets when identified as queries.

A ticket should store:

- Entity
- Location when resolved
- Raw message text
- Requested start time
- Requested end time
- Subject descriptor, for example `child in red shirt` or `customer with black bag`
- Status: `pending`, `in_progress`, `resolved`, or `escalated`
- Linked tracking session after an operator starts reviewing it
- WhatsApp message id

Managers should be encouraged to use a lightweight query format:

```text
QUERY
Entity: SSOM
Location: VR Station
From: 2026-07-06 14:10:00
To: 2026-07-06 14:25:00
Subject: child in red shirt
Need: confirm where he went after the ride
```

The importer can parse structured messages first. For casual text, it should reuse the same alias lookup used by voice commands for entities and locations, and use a proven date parser in the real app rather than inventing a fragile custom date system.

## Ticket Staging Workflow

When a ticket resolves to a location and time range:

1. Mark the ticket as `pending`.
2. Find all cameras mapped to that location.
3. Open those cameras in playback mode using the requested time range.
4. Show them prominently on the spatial canvas.
5. Start or link a tracking session when the operator begins review.
6. Save breadcrumbs as the operator moves through cameras.
7. Mark the ticket `resolved` or `escalated` when finished.

## Unified Spatial Canvas

Live view and playback should share the same spatial canvas behavior. A camera pin can show live video or playback video depending on mode, but pan, zoom, focus/peripheral/offscreen rules, pre-warm behavior, and location prominence should be shared.

This avoids building two separate map/grid systems and keeps operator muscle memory consistent.

## Implementation Order

1. Add schema tables for entities, locations, camera-location mapping, tag index, compliance types, compliance events, compliance logs, tickets, and operator settings.
2. Add prototype local state for entity/location/camera mappings.
3. Enforce `YYYY-MM-DD`, `HH:MM:SS`, and `YYYY-MM-DD HH:MM:SS` formatting in all forms and WhatsApp templates.
4. Add compliance overlay toggle and history window in the dashboard.
5. Add location/entity tags and top-three compliance labels to camera tiles.
6. Add compliance log creation with camera/location selection and manager response fields.
7. Add ticket schema and internal CRUD, with only a disabled dashboard placeholder until the external producer exists.
8. Add ticket-driven playback staging after tickets have a real producer.
9. Connect future external tools through the VMS API contract rather than direct multi-process SQLite writes.
