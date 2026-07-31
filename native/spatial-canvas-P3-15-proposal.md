# Spatial camera canvas — P3-15 slice 1 (+ P3-01 tile drag), first native pass

> Scope note for native increment 24. Subordinate to `Development_plan.md`.
> Owner directive 2026-07-31: "build the UI same or better than we had in the
> prototype" — the browser prototype's `spatial-canvas.js` is the reference.

## Problem & users

The operator needs to navigate cameras **spatially** — pan and zoom across a
canvas of camera tiles arranged to match the premises — instead of only a fixed
grid, with media cost following what is actually on screen.

## Included scope (this pass)

Port the reference prototype's spatial canvas semantics **exactly**, then
exceed them where the native runtime is stronger:

- **Same as prototype** (`spatial-canvas.js`):
  - pan the canvas by dragging empty space; **wheel zoom toward the cursor**
    (scale 0.15–6×); click = select/focus (drag-vs-click at 3 px); fit-to-view;
  - **zoom levels** site (<0.4) / wing (<0.9) / room, capping tiers
    site→paused, wing→thumb, room→main;
  - **zones** by screen position: focus (≤24% of min dimension from center) /
    peripheral (≤55%) / prewarm (within 240 px margin off-screen) / culled;
  - zone→tier: focused→main; focus zone→main, else thumb; prewarm→thumb;
    culled→paused — capped by the zoom level;
  - **anti-flap**: downgrades apply immediately; promotions wait until 300 ms
    after pan/zoom settles;
  - default world layout = the prototype's 320×190 grid placement.
- **Better than prototype:**
  - the resolved tiers feed the **real governor** (`applySpatialTiers`, one
    batched re-plan): budgets are the honest decode/memory/bandwidth admission
    already built — replacing the prototype's crude `maxActiveStreams` sort —
    and a culled tile is `visible=false` → **PausedOffscreen, actually not
    decoding** (the prototype only skipped drawing);
  - **per-tile drag** repositions a camera on the canvas (P3-01's drag verb;
    the prototype could only pan the whole canvas);
  - positions **persist** across restarts (schema v9: `workspace_tile.pos_x/
    pos_y`, forward-only additive; default −1 = unset → prototype grid layout);
  - honest per-tile state chrome (the P3-14 colors) on every spatial tile.
- A **Grid | Spatial** toolbar toggle on the Live workspace (`--spatial` starts
  in spatial mode so the offscreen smoke exercises it). Auto-sweep pauses in
  spatial mode (the viewport, not a sweep, owns the working set).
- A pure `SpatialPolicy.h` core (zones, zoom levels, tier resolution, dwell) so
  the policy is unit-tested headlessly: `--spatial-selftest`.

## Excluded (own later gates)

- Live video composited under the spatial tiles (the d3d11 grid compositor is
  fixed-geometry; per-tile video items on the canvas are a later media slice —
  the prototype likewise drew snapshots, not live video, on the canvas).
- Floor-plan/map images under the canvas, camera facing/field-of-view cones,
  map pins vs tiles, and the premises/site entity (rest of P3-15).
- Saved/personal/shared named layouts (rest of P3-01); RBAC (P1-02).

## Acceptance criteria (offscreen-verifiable)

1. Policy parity: zones, zoom levels, tier caps, and dwell reproduce the
   prototype's `resolveTierByZone`/`shouldApplyTierChange` semantics.
2. `updateSpatialViewport` applies a viewport pass in **one** batched re-plan; a
   culled tile shows the honest `paused-offscreen` state (no decode); a
   no-change pass does not re-plan.
3. Dragging a tile updates its world position; positions survive a save/load
   round-trip (schema v9 migrates from v8 cleanly).
4. The offscreen QML smoke loads the spatial canvas (`--spatial`) with no QML
   errors/warnings; pan/zoom/drag handlers bind.
5. All prior self-tests stay green.
