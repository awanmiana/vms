# Shared Data Layer Architecture

The VMS, compliance posting formatter, WhatsApp extraction/formalization tool, compliance logging UI, and future live assistant should not each invent incompatible meanings for the same operational data.

"Shared" and "canonical" in this document describe one logical data contract and stable identifiers. They do not require one physical central database. Standalone, coordinated, replicated, or federated placement—and synchronization authority between them—remain subject to P0-04 in the controlling roadmap.

They should share one canonical data layer for:

- Entities
- Locations
- Camera-location mappings
- Tag index
- Compliance types
- Compliance events
- Compliance logs
- Query tickets
- Voice/search aliases

## Why This Matters

If each system keeps its own entity names, location names, and compliance type labels, they will drift quickly. `Sale not punched`, `sales not being punched`, and `sale not being entered` would become separate meanings in separate tools.

The shared layer keeps one canonical id for each business concept, while still allowing aliases and tags for operator-friendly search.

## System Boundaries

The VMS should own camera viewing, playback staging, spatial navigation, stream limits, and tracking breadcrumbs.

The compliance posting formatter should help operators post structured WhatsApp messages using the exact date/time format and approved compliance types.

The WhatsApp extraction/formalization tool should read designated groups, classify posts and replies, link them to known entities/locations/types, and write structured records.

The compliance logging UI should let operators confirm or correct extracted data, select involved cameras and locations, and log manager responses.

The live assistant should control these tools through command interfaces instead of bespoke UI automation whenever possible.

## Command Pattern

The VMS already uses a shared command shape. The other tools should expose similar command layers:

- `CREATE_COMPLIANCE_LOG`
- `EDIT_COMPLIANCE_LOG`
- `CREATE_TICKET`
- `RESOLVE_TICKET`
- `ESCALATE_TICKET`
- `EDIT_COMPLIANCE_TYPE`
- `MERGE_COMPLIANCE_TYPE`
- `OPEN_LOCATION_PLAYBACK`

The assistant can then route user intent into commands without special-case logic for every screen.

## Date and Time Contract

All systems must use:

- Date: `YYYY-MM-DD`
- Time: `HH:MM:SS`
- Timestamp: `YYYY-MM-DD HH:MM:SS`

This contract should appear in UI placeholders, WhatsApp templates, validation, import rules, and database documentation.

## Recommended Build Sequence

1. Finalize the shared schema.
2. Add strict date/time validation to the posting formatter and VMS forms.
3. Add entity/location/tag search in the VMS prototype.
4. Add compliance log creation and manager-response fields.
5. Feed extracted WhatsApp records into the shared schema.
6. Add ticket-driven playback staging.
7. Add assistant commands across VMS and compliance tools.
