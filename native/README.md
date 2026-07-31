# VMS Native

Fully-native C++ VMS client. See [`ARCHITECTURE.md`](ARCHITECTURE.md) for the committed stack
and all decisions. The browser/Node code in the repository root is retained as a reference
specification, not the product runtime.

> ⚠️ Native C++ is authored here but compiled/run on your hardware. If a build surfaces
> compiler/linker errors, paste them and they get fixed.

## Current status (2026-07-27)

Foundation validated. Increments 1–3 build and run; self-contained packaging is proven. Increment
4a (`vms_grid`) is verified on a live camera and its camera-free `--test-pattern` fan-out measured
the decode ceiling per tier on the dev box (see `MEASUREMENTS.md`: ~15 main / ~63 sub 1080p H.265
streams; memory is the hard wall — 64 tiles crashed at 10.5 GB). P3-03 governor scope is approved
(`governor-P3-03-proposal.md`) and increment **4b** has begun: the governor decision core
(`vms_govtest`) is built and registered with CTest; the grid integration now probes the current
machine by default, verifies per-codec D3D capability plus an installed GStreamer hardware decoder,
and builds a bounded capacity profile. The corrected camera-typical I420, auto-probed 64-tile H.265
case caps to 1 main + 47 sub + 16 thumb and runs at 3.77 GB instead of crashing. The stateful
session's changing tier plans are now **applied to live media branches** (`vms_grid --sweep`): a
runtime focus sweep re-plans, `DiffPlans` orders each change release-before-acquire, and the grid is
reconfigured to the new plan — verified on hardware (all tiers resume, memory bounded, clean exit).
The governor's tier decisions now also **select real per-camera RTSP streams** (`--govern --camera
"main;sub"`): Main opens the main stream, Sub/Thumb the sub stream, Paused opens no session. The
tier→stream mapping, the credential-free `stream selection:` line, and graceful per-tile connection
handling are verified locally; live-camera decode confirmation needs a run with the camera's
credentials. The dev-box cost model was **recalibrated** against the corrected I420 `d3d12h265dec`
ramp (see `MEASUREMENTS.md`): ceilings ~70 main / ~249 sub, decode-throughput-bound (not memory-
bound — the old 10.5 GB "wall" was a pre-fix NVDEC artifact), so the decode budget rose 15→64 and
`--govern --profile auto` now keeps 25 of 64 tiles at full main resolution instead of 1. The
**honest per-tile state model** (live / degraded / paused-offscreen / paused-capacity) is built and
shown live (`tile 0 [main/live]`, `tile 5 [thumb/degraded]`). Its *visual* rendering — the **P3-14
Qt/QML gate, slice 1** — is now **built and verified on the dev box**: `vms_workspace` is a Qt Quick
window that binds the real `GovernorSession` to a grid whose per-tile border + status strip carry the
honest state (green live / amber degraded / red capacity-paused / slate off-screen), with an aggregate
capacity meter and an over-capacity warning; a focus sweep re-plans live. This realizes P3-03
acceptance #5 (a non-live tile is never shown as if it were live) on screen and not only in a log.
It depends only on Qt Quick + the pure `vms_governor` library, so it verifies headlessly via
`--selftest` and windowed. **Slice 2a is now built and verified too:** the gst→Qt Quick video path
(`--video`) — a GStreamer `appsink` pulls finished RGBA frames and a `VideoItem` (`QQuickItem`)
uploads each to a scene-graph texture, so the honest state chrome draws *over live video*. `qml6glsink`
is absent from the MSVC GStreamer binaries, so this `appsink`→`QSGTexture` route is the deliberate,
interop-free bridge. **Slice 2b-1 is now built and verified too:** `GridPipeline` makes the governed
d3d11-composited grid the real video source (per-tile hardware decode → `d3d11compositor` →
`d3d11download` → `appsink` → `VideoItem`), built from the governor's initial plan, chrome aligned
cell-for-cell, paused tiles composited black. Verified on the dev box: 16 tiles decode real H.265
(`d3d12h265dec`, hardware) under the honest chrome (lowend plan: 1 main/live + 1 sub/degraded + 14
thumb/degraded, aligned). **Slice 2b-2 is now built and verified too:** a focus sweep re-plans BOTH
layers in lock-step — the controller re-plans the chrome and `GridPipeline::applyPlan` reconfigures the
live video to the same tiers (whole-graph NULL→rebuild-fronts→PLAYING; sweepable grids pre-encode every
tier; a no-op when nothing changed). Verified with `--video --sweep-interval 3`: the MAIN/focused tile
moves across the grid in step with the chrome, `d3d12h265dec`, no re-plan errors. **Slice 2b-3 is now
built and verified too:** `--profile auto` (now the default) seeds the governor from the live hardware
probe exactly as `vms_grid` does — on the dev box it reports `auto h265 (High, hardware decode, NVIDIA
GeForce RTX 3050)` and plans 25 main + 39 sub of 64, matching `vms_grid`. This completes the P3-14
slice-2 group (video under the honest chrome). **Slice 3 adds the first workspace interaction:**
clicking any tile focuses it (`focusTile`) — the governor protects it at Main and degrades the rest
around it, a `planChanged` signal makes both the chrome and (under `--video`) the live picture follow,
and a layered "selected-camera information" panel floats over the grid (id · tier · state · priority);
a click stops the auto-sweep so the operator holds the working set. **Slice 4 adds the layered resource
browser and toolbar:** a camera-list panel with a live search filter layers over the grid — each row
(state dot · `Cam N` · tier·state) click-focuses that camera and the focused one is highlighted — and a
toolbar toggles the browser and the auto sweep. Pure Qt Quick primitives (no new Qt module); verified
over the live 16-tile governed video. **Slice 5 adds the second orthogonal control axis:** operator-
settable device priority — decoupled from focus so it persists across sweeps, set via High/Med/Low
buttons in the info panel (`setPriority`), with a ★ badge on High cameras; under capacity pressure the
governor degrades lower-priority cameras first (verified: a non-focused High camera holds Main while a
Low one is paused). **Slice 6 adds runtime layout control:** toolbar presets (4/9/16/25/64) resize the
wall live (`setTileCount`) — the working set is rebuilt, the governor re-plans, and under `--video` the
grid pipeline is rebuilt for the new geometry (cached clips make it instant); fewer tiles let the
governor upgrade more to Main (verified: 2 main at 4 tiles vs 1 main at 16). **Slice 7 makes the first
control axis interactive:** operator-set desired media tier (Main/Sub/Thumb/Off in the info panel,
`setDesiredTier`) — the governor never exceeds the ceiling, so capping a camera or turning it Off frees
budget for the others (verified: a Thumb cap holds at thumb, Off pauses, even with budget to spare).
Both orthogonal axes from ARCHITECTURE.md (media tier + device priority) are now operator-controllable.
**Slice 8 realizes the P3-14 headline — two independently stateful instances:** a Live/Playback tab bar
with two `WorkspaceController` instances, `governor` bound to the active tab so the whole view re-binds
with no duplication; the video pipeline follows Live, and Playback is chrome-only with a transport
scaffold honestly marked "awaiting recorded footage (Phase 4)". **Next up: wiring the Playback
range/timeline/transport to real recordings (needs the Phase-4 recording backend) and a seamless
(non-reloading) live apply; (blocked on hardware) live-camera RTSP confirmation and low-end i5
calibration.**

The **connection broker + credential store (DPAPI)** gate — the gate P0-04 deferred its
`credential_ref` to — is now **scope-approved (2026-07-27, Option A: in-process broker + Windows
DPAPI)** per `credential-broker-P1-03-proposal.md`, and **increment 6a is built and verified on the
dev box:** `vms_persist` gains a swappable, SQLite-free `SecretStore` (a shared `Error.h` now carries
the typed status vocabulary) with a **Windows DPAPI** backend (`CryptProtectData`/`CryptUnprotectData`,
ciphertext base64-stored in an atomically-rewritten backing file, plaintext only in memory) and an
in-memory test double. `vms_credtest`/CTest (`credential_selfcheck`) passes 26 checks: the interface
contract, a real DPAPI encrypt→decrypt round-trip, the **no-plaintext-at-rest** property (the on-disk
file holds only ciphertext), secret durability across reopen, idempotent remove, and typed
`NotFound`/`Crypto`/`Misuse` failures. **Increment 6b is now built and verified too:** a
`CredentialRepo` binds `device_credentials.credential_ref` ↔ `SecretStore` so SQLite holds only the
opaque ref and the plaintext secret never enters any DB file. Provisioning is atomic — both the ref
row and the encrypted secret are written or neither, verified on both failure paths (a failed secret
write rolls back the ref row; an unknown-device FK violation writes no secret) — the ref is stable
across secret rotation, and remove is atomic + idempotent. `vms_credtest` now runs 45 checks
(6a + 6b), including a no-plaintext scan of the main + WAL + shm DB files. **Increment 6c-1 (the
broker core) is now built and verified too:** a pure, Qt-free/GStreamer-free `ConnectionBroker`
(`src/broker/`) is the single device-connection identity — `connect(camera_id, stream)` resolves the
camera's device + credential-free URL template from SQLite, resolves the device secret through the
`CredentialRepo`, materializes the credentialed URL **in memory** (userinfo percent-encoded; never
logged, never persisted), enforces per-device connection **pooling**, and applies the P0-03 login
**backoff / circuit-breaker**, failing with honest typed errors (`NotFound`/`Crypto`/`Unavailable`/
`Misuse`) — never a silent retry or a false "connected". `vms_brokertest`/CTest passes 32 checks
(materialization + encoding, tier→stream selection, typed failures, pooling with release, and the
full breaker open→refuse→cooldown→recover cycle via an injected clock). **Increment 6c-2 is now
built and verified too:** `vms_grid` connects through the broker by `camera_id` — `--broker-db
<path>` + `--provision "id|user:pass|main[|sub]"` provision a camera (device + credential-free URL
templates + DPAPI-capable secret), and the governor's per-tier stream selection resolves each
camera's Main/Sub URL through the `ConnectionBroker` (materialized in memory, never on the run CLI).
A headless `--broker-selftest` proves the wiring (5 checks: percent-encoded credentialed URL
materialized in memory, main/sub template selection, honest failure on an unknown camera_id); the
raw-URL `--camera` path is retained as a documented diagnostic. `vms_workspace` has **no
URL-with-password path** to rewire (its `GridPipeline` decodes pre-encoded synthetic clips, not RTSP)
— it will connect through the broker when it gains real camera video (a later media gate). This
completes the broker gate's standalone scope; live-camera decode confirmation shares the existing 4b
hardware/credential block.

| # | Component | Status |
| :- | :- | :- |
| 1 | `vms_hwprobe` — D3D11 per-codec HW-decode probe + machine tier | ✅ verified (dev box) |
| 2 | `vms_shell` — Qt6 + GStreamer window (toolchain proof) | ✅ verified |
| 3 | `vms_spike` — RTSP → HW decode → D3D11 render + stats | ✅ verified (live camera) |
| — | Self-contained packaging (bundle Qt+GStreamer; user installs nothing) | ✅ proven (ran with cleaned PATH) |
| 4a | `vms_grid` — N streams → per-tile HW decode → `d3d11compositor` → one window; per-tile fps + composited rendered/dropped + process CPU/RAM; camera-free `--test-pattern` fan-out | ✅ verified on live camera (4-tile); fan-out ramp + low-end run pending |
| 4b | Decode governor — `vms_governor` decision core + stateful session, `vms_govtest`/CTest, and `vms_grid --govern` integration | 🟡 in progress; policy core passes at MSVC `/W4`; add/remove/focus re-planning and low/high-water hysteresis pass; **auto-probed decode integration verified on hardware: 64 requested main tiles become 1 main + 47 sub + 16 thumb and the corrected I420 path runs at 3.77 GB (`d3d12h265dec`) instead of crashing at 10.5 GB.** Changing plans are now **applied to live branches** via `vms_grid --sweep` (whole-grid NULL→rebuild on each plan change; seamless per-tile hot-swap deferred — it does not survive the D3D12 decoder + reused compositor pad). The governor's tiers also **select real RTSP main/sub streams** (`--govern --camera "main;sub"`; plumbing verified locally, live-camera decode pending). The cost model was **recalibrated** to the corrected I420 `d3d12h265dec` ramp (budget 15→64, sub cost 0.238→0.28, VRAM-tiered auto seed; auto now plans 25 main + 39 sub of 64, CTest green). The **honest per-tile state model** (live/degraded/paused-offscreen/paused-capacity) is built, unit-tested, and shown live. P3-03 scope approved 2026-07-24. Remaining: P3-14 slice 2 (live video under the state chrome + auto profile seed), seamless apply, and (hardware-blocked) live-camera RTSP confirmation + low-end i5 calibration |
| P3-14 s1 | `vms_workspace` — Qt Quick window rendering the governor's honest per-tile state (border + status strip: green live / amber degraded / red capacity-paused / slate off-screen) + aggregate capacity meter + over-capacity warning; live focus-sweep re-plan; binds real `GovernorSession`; no media pipeline yet | ✅ built & verified (dev box): builds at Qt6 Quick 6.11.1, `--selftest` green (devbox 64→55 main + 9 sub degraded; lowend re-plan release-before-acquire), window renders all four states distinctly (see run section). Realizes P3-03 acceptance #5 on screen |
| P3-14 s2a | gst→Qt Quick video path: `VideoItem` (`QQuickItem`) + a GStreamer `appsink` (`--video`); frames go `appsink → QImage → QSGTexture`, chrome drawn over live video. `qml6glsink` is absent from the MSVC binaries, so this interop-free route is the chosen bridge | ✅ built & verified (dev box): `videotestsrc` SMPTE renders full-window behind the translucent governed tiles; default (non-video) path unchanged. Source is a synthetic test pattern — governed d3d11 grid is s2b |
| P3-14 s2b-1 | The governed d3d11-composited grid IS the video source (`GridPipeline`): per-tile hardware decode → `d3d11compositor` → `d3d11download` → `appsink` → `VideoItem`, built from the governor's initial plan; chrome aligned cell-for-cell (equal fractions, zero-gap); paused tiles composited black (never a stale frame) | ✅ built & verified (dev box): 16 tiles decode real H.265 (`decoders: d3d12h265dec`, hardware) under the honest chrome — lowend plan shows 1 main/live + 1 sub/degraded + 14 thumb/degraded, aligned. Note: the binary now links GStreamer, so its DLLs must be on PATH even for `--selftest` |
| P3-14 s2b-2 | Live re-plan on a focus sweep: one timer drives BOTH layers — the controller re-plans the chrome and `GridPipeline::applyPlan` reconfigures the live video to the same tiers (whole-graph NULL→rebuild-fronts→PLAYING, the reliable path; sweepable grids pre-encode every tier). No-op when nothing changed (no flicker at steady load) | ✅ built & verified (dev box): with `--video --sweep-interval 3` the MAIN/focused tile moves 0→1→2… in lock-step across chrome and picture (captured), console logs each `sweep -- focus -> tile N; K transition(s)`, `d3d12h265dec`, no re-plan errors |
| P3-14 s2b-3 | `--profile auto` (now the default): seed the governor from the live hardware probe — D3D per-codec capability + a registered GStreamer hardware decoder — exactly as `vms_grid` | ✅ built & verified (dev box): reports `auto h265 (High, hardware decode, NVIDIA GeForce RTX 3050)`, plans 25 main + 39 sub of 64 (matches `vms_grid`). Completes the P3-14 slice-2 group |
| P3-14 s3 | Interactive workspace: click any tile to focus it (`focusTile`) → the governor protects it at Main and degrades the rest around it; a `planChanged` signal makes both chrome and (under `--video`) the live picture follow; a layered "selected-camera information" panel floats over the grid (id · tier · state · priority). A click stops the auto-sweep (operator in control) | ✅ built & verified (dev box): `--selftest` proves `focusTile` promotes the picked tile to Main; a simulated click moved focus+MAIN to the clicked tile with chrome, live video, and the info panel all following (captured). This is the first operator→governor interaction and the first layered panel of the P3-14 workspace |
| P3-14 s4 | Layered resource browser + toolbar: a camera-list panel (with a live search filter) layers over the grid — each row shows a state dot + `Cam N` + tier·state and click-focuses that camera, mirroring the tile; the focused camera is highlighted. A toolbar toggles the browser and the auto sweep (`setAutoSweep`, `autoSweeping`). Pure Qt Quick primitives — no new Qt module | ✅ built & verified (dev box): browser list state matches the tiles exactly (Cam 0 main/live, Cam 1 sub, rest thumb/degraded), search filters, Auto-sweep toggle reflects state; captured over the live 16-tile governed video with the info panel. CTest + `--selftest` green |
| P3-14 s5 | Operator-settable device priority — the **second orthogonal control axis** (ARCHITECTURE.md). Priority is decoupled from focus (persists across sweeps); High/Med/Low buttons in the info panel set it (`setPriority`), and a ★ badge marks High cameras on tiles and in the browser. Under capacity pressure the governor degrades lower-priority cameras first | ✅ built & verified (dev box): `--selftest` shows a **non-focused High** camera holds Main while a Low one is paused (tile 1 High=main, tile 15 Low=paused on lowend-16); a simulated High click highlights the button and shows `Cam 0 ★`. CTest + `--selftest` green |
| P3-14 s6 | Runtime layout control: toolbar presets (4/9/16/25/64) resize the wall live (`setTileCount`) — rebuilds the working set, re-plans, and (under `--video`) rebuilds the grid pipeline for the new geometry via a `layoutChanged` signal; encoded clips are cached so the rebuild is instant | ✅ built & verified (dev box): `--selftest` proves `setTileCount(4)`→2×2/4 tiles; a simulated click on the "4" preset rebuilt the 16-tile wall to 2×2 live, and the governor upgraded more tiles to Main (2 main + 2 sub at 4 tiles vs 1 main at 16) — real decoded video throughout. CTest + `--selftest` green |
| P3-14 s7 | Operator-set desired media tier — the **first orthogonal control axis** made interactive. Main/Sub/Thumb/Off buttons in the info panel set a camera's quality ceiling (`setDesiredTier`); the governor never exceeds it (it seeds each tile at `desired`), so capping a camera or turning it Off frees decode+memory budget for the others | ✅ built & verified (dev box): `--selftest` shows a Thumb cap holds a camera at thumb and Off pauses it even with budget to spare; a simulated Thumb click capped the focused camera to THUMB·Live live (info panel + browser reflect it). CTest + `--selftest` green |
| P3-14 s8 | **Two independently stateful instances** (the P3-14 headline): a Live/Playback tab bar with two `WorkspaceController` instances; QML `governor` binds to the active tab so the whole view re-binds with no duplication. The video pipeline follows the Live instance; the Playback tab is chrome-only with a transport scaffold (timeline + ⏮◀◀▶▶▶⏭) honestly marked "awaiting recorded footage (Phase 4)" | ✅ built & verified (dev box): captured both tabs — Live shows the video-backed governed grid, Playback shows its own independent state + transport bar, video hidden; switching re-binds the entire workspace. CTest + `--selftest` green |
| 5 (P0-04) | Persistence — the standalone embedded store (owner-selected direction). `vms_persist`: SQLite via the Windows-bundled **winsqlite3** (no download/vendoring; Qt-free, swappable) behind the C0-03 adapter contract — declared tables, atomic writes + RAII rollback, versioned idempotent forward migrations (`Schema.cpp`, one source of truth), typed errors, online backup/export; a `WorkspaceRepo` for layout state; and `vms_workspace` wired to persist/restore. `vms_dbtest`/CTest | ✅ built & verified (dev box): `persist_selfcheck` green — 29 checks (canonical schema, durability across reopen, FK constraint, atomic rollback, layout round-trip, export). **5c end-to-end verified:** close the app with a 25-tile 5×5 layout, relaunch requesting `--count 4`, and it restores 25 tiles. Scope approved 2026-07-26 (`persistence-P0-04-proposal.md`). Central DB/cloud/sync + offline conflict reconciliation deferred to P1-10; credential secrets (DPAPI) are the next gate |
| 6a (P1-03/N0) | Credential store — the OS secret store (DPAPI). `vms_persist` gains a swappable, SQLite-free `SecretStore` interface (shared `Error.h` typed status) with a **Windows DPAPI** backend (`CryptProtectData`/`CryptUnprotectData`; ciphertext base64 in an atomically-rewritten backing file; plaintext in memory only) and an in-memory double. Realizes the N0 broker decision + P1-03 secrets-storage clause for the standalone runtime. `vms_credtest`/CTest | ✅ built & verified (dev box): `credential_selfcheck` green — 26 checks (interface contract, real DPAPI encrypt→decrypt round-trip, **no-plaintext-at-rest**, durability across reopen, idempotent remove, typed `NotFound`/`Crypto`/`Misuse`). Scope approved 2026-07-27, Option A (`credential-broker-P1-03-proposal.md`). Coordinated vault/RBAC → P1-10, operator auth/MFA → rest of P1-03, credential audit → P1-06 |
| 6b (P1-03/N0) | Credential wiring + atomicity. A `CredentialRepo` (`CredentialRepo.h/.cpp`, in `vms_persist`) binds `device_credentials.credential_ref` ↔ `SecretStore`: SQLite holds only the opaque ref, the plaintext secret never enters any DB file, and provisioning writes the ref row + encrypted secret **atomically** (both or neither). `vms_credtest`/CTest | ✅ built & verified (dev box): `credential_selfcheck` now 45 checks (6a + **19 for 6b**) — round-trip, opaque-ref, stable ref across rotation, both atomicity paths (failed secret write rolls back the ref row; unknown-device FK violation writes no secret), atomic + idempotent remove, and a **no-plaintext scan of the main + WAL + shm DB files**. |
| 6c-1 (P1-03/N0) | The standalone `ConnectionBroker` (`src/broker/`, lib `vms_broker`) — the single device-connection identity. Pure C++ (no Qt/GStreamer): `connect(camera_id, stream)` resolves device + credential-free URL template from SQLite, resolves the secret via `CredentialRepo`, materializes the credentialed URL **in memory** (userinfo percent-encoded, never logged/persisted), enforces per-device **pooling** and the P0-03 **backoff/circuit-breaker**, and fails with honest typed errors. `vms_brokertest`/CTest | ✅ built & verified (dev box): `broker_selfcheck` green — 32 checks (materialization + `%`-encoding, tier→stream selection, `NotFound`/`Crypto`/`Unavailable`/`Misuse` failures, pooling with release, full breaker open→refuse→cooldown→recover via injected clock). |
| 7a+7b (P4-03/P5-02) | Recording data authority. Schema v4 `segments` + `SegmentIndex` (`vms_persist`): atomic `add` with validation (never index unwritten footage), overlap `list`, `totalBytes`, and an oldest-first size/age retention cap that returns trimmed segments for file cleanup (crude stand-in for P4-06); plus `ComputeAvailability` — honest available/missing-gap/overlapping spans over a requested range (P5-02, requested time never shown as recorded). Optional local recording only (P0-01E). `vms_rectest`/CTest | ✅ built & verified (dev box): `recording_selfcheck` green — 29 checks (add/validation, overlap listing, size+age retention oldest-first, and availability over continuous / gapped / edge / empty / overlapping ranges). All 5 CTest suites green. Scope approved 2026-07-29 (Option A). **7c** (`vms_workspace` Playback wired to segments + real transport) remains |
| 7a-media (P4-03) | Recording pipeline. `vms_record` (`src/record/`, GStreamer + `vms_persist`): a source is muxed into fixed-duration segment files by `splitmuxsink`, and every completed fragment is written into the `SegmentIndex` with its wall-clock [start,end], path, codec, and byte size (SQLite touched only on the main thread after EOS). Off unless run; optional local recording (P0-01E) | ✅ built & verified (dev box): a 12s `--test-pattern --segment-seconds 3` run wrote **4 real `.mp4` files** (~45 KB each) and indexed them as **one contiguous Available span** — record→index→availability proven end-to-end with no camera. The real-camera path (store original codec via depay through the broker) is the live-blocked remainder |
| 7c-1 (P5-02) | Playback data model. `PlaybackController` (`src/workspace/`, Qt + `vms_persist`) turns the `SegmentIndex` into the honest model the Playback timeline binds to: availability spans over a requested window (available/missing-gap/overlapping as fractions), a draggable playhead reporting footage-vs-gap and resolving the backing recorded file + offset, and transport state (play/pause/speed/frame-step). `vms_workspace --playback-selftest` | ✅ built & verified (dev box): `--playback-selftest` green — 12 checks (3 spans available/missing/available, playhead over footage vs gap, `segmentAtPlayhead` resolves the file, requested time in a gap never shown as recorded, transport state). **7c-2** (bind the spans to the QML Playback timeline + transport) is built & verified too |
| 7c-2 (P5-02) | Playback timeline + transport, bound to real footage. The QML Playback tab binds to a recording-backed `PlaybackController` (`--rec-db`/`--rec-camera`): the timeline draws real availability bands (green available / amber overlapping / dark gaps — requested time never painted as recorded) with a draggable playhead reporting recorded time + footage-vs-gap; transport buttons (⏮ ◀ᖴ ▶/⏸ ᖴ▶ ⏭ + 1×/2×/4×) drive the controller. New `--smoke-ms` loads the scene offscreen so QML bindings are build-verifiable without a display | ✅ built & verified (dev box): offscreen smoke (`QT_QPA_PLATFORM=offscreen --smoke-ms 1500`) against a real `vms_record` DB loads the scene, binds 1 footage span, and exits clean with **no QML errors/warnings**. On-screen appearance is a windowed run. **7c-3** (decoded video in the pane) is built & verified too |
| 7c-3 (P5-02) | Decoded playback video. `PlaybackPipeline` (`src/workspace/`, `filesrc`→`decodebin`→`videoconvert`→RGBA→`appsink`→`VideoItem`, real-time paced, seek/rate) decodes the recorded segment at the playhead into a Playback `VideoItem`; the controller's scrub/play/pause/speed drive it, the decoded position pushes the timeline playhead back, and it rolls onto the next segment file at a boundary. `vms_record` now stores **absolute** segment paths (location-independent index) | ✅ built & verified (dev box): an offscreen smoke (`--smoke-ms 3500 --rec-db …`) that seeks to start + plays pulled **85 real decoded frames** from the recorded `.mp4` files with no pipeline errors (0 before the absolute-path fix). All 5 CTest suites green. On-screen motion is a windowed run. **Increment 7's standalone scope is complete**; only the real-camera record path (original codec via depay through the broker) remains — live-blocked |
| 8 (P12-03/P0-01F) | Optimization service — the "watch for the bottleneck" feature. A Qt-free swappable `SystemHealthProbe` (`src/optimize/`, Windows impl reads live RAM + system CPU load; injectable fake) + an `Optimizer`/`OptimizerSession` mapping live headroom onto the governor's `CapacityProfile` (never above the static ceiling; tightens immediately, deadbands recovery so the budget can't flap); a **bandwidth budget** added as a 3rd governor admission budget (`0` = unmodeled, so two-budget behavior is unchanged); and an honest read-only `OptimizationReport` naming the binding budget. `vms_opttest`/CTest | ✅ core built & verified (dev box): `optimize_selfcheck` green — 16 checks (ample=no-clamp, RAM pressure degrades **before** exhaustion, recovery deadband vs immediate tighten, bandwidth admission + attribution, and the real Windows probe reporting live free-RAM/cores). All 6 CTest suites green. Scope approved 2026-07-29 (Option A). **Live sampler wired & verified too:** `vms_workspace` runs a `QTimer` health sampler that adjusts the Live wall's capacity live (`applyOptimizedProfile`) and shows the read-out in the header (`--opt-interval`, `--no-optimize`, and `--opt-fake-ram`/`--opt-fake-bw` for testing under injected pressure). Verified offscreen: injecting 400 MB free RAM degraded the running 16-tile wall to `decoding 9/16` under a clamped `mem 240 MB (ram)` budget; real ample headroom kept all 16 at main. **Remaining:** true active bandwidth probing (link budget is operator/estimated for now); OS-level always-on service + node failover are their own gates |
| 10 (P2-01/02/05) | Device onboarding inventory layer. `DeviceRepo` (`src/persist/`) on the P0-04 store: atomic onboard of a recorder/direct camera + channels + credential (secret via `SecretStore` under the broker's `credential_ref` scheme); one protected auto **default device group** per device; **idempotent channel sync** preserving stable camera ids + operator names. Standards-correct inventory, **not a protocol mock** — real ONVIF/LAN discovery (P2-03/P2-14) feeds it later. `vms_devtest`/CTest | ✅ built & verified (dev box): `device_selfcheck` green — 26 checks (atomic onboard, auto group, idempotent re-onboard, operator-name-preserving sync with add/remove, and an **onboard→ConnectionBroker end-to-end resolve**: the onboarded camera is immediately connectable). All CTest suites green. Scope approved 2026-07-29 |
| 11 (P2-03/P2-14) | ONVIF **WS-Discovery** — standards-exact discovery source for `DeviceRepo`. `vms_onvif` (`src/onvif/`, Qt6 Core/Network): pure `BuildProbe` (WS-Discovery 2005/04 SOAP-UDP, `dn:NetworkVideoTransmitter`) + prefix-agnostic `ParseProbeMatches` (EndpointReference/Types/Scopes/XAddrs) + `DedupByEndpoint` + `ScopeValue`; a `QUdpSocket` `Discover(timeout)` multicast transport. Real ONVIF wire format. `vms_onviftest`/CTest (needs Qt DLLs on PATH) | ✅ WS-Discovery core built & verified (dev box): `onvif_selfcheck` green — 16 checks (standards-exact Probe, ProbeMatch parse across real Hikvision + different-prefix Axis fixtures, XAddrs + percent-decoded scopes, dedup, honest empty). All 8 CTest suites green. **Remaining (live/hardware):** `Discover()` on the LAN finding the test camera |
| 12 (P2-14) | ONVIF **device/media SOAP client** — completes discover→onboard. `OnvifClient` (`src/onvif/`): WS-Security UsernameToken **PasswordDigest** + `GetDeviceInformation`/`GetProfiles`/`GetStreamUri` builders + prefix-agnostic parsers; `OnvifTransport::FetchDevice` (Qt Network) POSTs them and returns info+profiles+RTSP URIs for `DeviceRepo.onboard`. Real ONVIF wire format | ✅ core built & verified (dev box): `onvif_selfcheck` now 25 checks incl. PasswordDigest = Base64(SHA1(nonce+created+password)) recomputed independently + password-sensitive, well-formed GetProfiles/GetStreamUri envelopes, and profile/stream-URI/device-info parsing against real fixtures. All 8 CTest suites green. **Remaining (live/hardware):** `FetchDevice()` against the test camera → `DeviceRepo.onboard` |
| 13 (P2-13) | Device health & lifecycle. Pure Qt-free `HealthMonitor` (`src/health/`): separate honest dimensions (reach/stream/storage) → derived state (Unknown/Online/Degraded/Offline/Unsupported); exception lifecycle (raise w/ first-seen → acknowledge → auto-clear on recovery); maintenance windows suppress the alert not the state; firmware inventory; `needsAttention()`/`shouldNotify()`; injectable clock. `vms_healthtest`/CTest | ✅ built & verified (dev box): `health_selfcheck` green — 22 checks (Unknown-until-observed, dimension independence, exception raise→ack→auto-clear, maintenance suppression, unsupported + firmware). All 9 CTest suites green. **Surfaced in the Onboarding UI (increment 14).** Remaining: feed it live from broker outcomes + ONVIF polling |
| 14 (P2-01/02/05/13) | **Onboarding UI** — the operator-facing device-management surface (owner item #3, after ONVIF ✅ + device health ✅). `DeviceController` (`src/workspace/`, Qt) bridges the two pure cores to QML: `DeviceRepo` (increment 10 — atomic onboard / channel sync / remove over the P0-04 store + DPAPI `SecretStore`) and `HealthMonitor` (increment 13 — honest per-device reach/stream/storage → state, exception raise→ack→auto-clear, maintenance). A new **Devices tab** in `vms_workspace` lists each onboarded device with its honest health (green Online / amber Degraded / red Offline / slate Unsupported / grey Unknown-until-observed), an onboard form (single-channel direct camera), and per-device acknowledge / maintenance / remove. New `DeviceRepo::listDevices` read accessor. `--devices-selftest` + offscreen `--smoke-ms --devices-demo` | ✅ built & verified (dev box): `--devices-selftest` green — 17 checks (onboard via the controller, credential stored only via the SecretStore under the broker ref, Unknown-until-observed, reported Offline→exception→needs-attention, acknowledge silences but keeps the exception, recovery auto-clears, remove deletes the secret). Offscreen `--smoke-ms --devices-demo` loads the whole scene + Devices tab with **no QML errors/warnings** (5 demo devices, 2 need attention). All 9 CTest suites green. **Remaining (live/hardware):** feed health from real broker connect outcomes + ONVIF polling (the `reportReach/Stream/Storage` wiring points), and the on-screen windowed view; discovery-as-a-source (P2-03/14) is its own slice. (NVR/hybrid multi-channel onboarding is increment 15.) |
| 15 (P2-01) | **NVR/hybrid multi-channel onboarding** — the recorder workflow, completing P2-01 alongside the direct-camera path. Schema **v5** adds `devices.kind` (`camera`\|`nvr`\|`dvr`\|`hybrid`, a forward-only additive migration). `DeviceOnboard`/`DeviceSummary` carry `kind`; `DeviceController::onboardRecorder` expands a **channel count + per-channel RTSP URL templates** (`{ch}` → the 1-based channel number) into N channels with stable ids (`<id>-ch<N>`) and onboards them **atomically** with the device, its default group, and its credential. The Devices-tab form gains a kind selector (recorder kinds show channel-count + template fields); cards show the kind + channel count. `--devices-selftest` + offscreen `--smoke-ms --devices-demo` | ✅ built & verified (dev box): `--devices-selftest` now green at **24 checks** (adds: schema v5, recorder onboard succeeds, kind persisted as `nvr`, 4 channels expanded from the templates with stable ids `nvr-1-ch1..ch4`, default group reconciled to all 4, idempotent re-onboard grows to 6). Offscreen smoke seeds 5 cameras + 1 NVR (8 channels) with no QML errors/warnings. All 9 CTest suites green (dbtest schema pinned to v5). **Remaining:** ONVIF `GetProfiles` as the channel source (live-blocked), and disabling/removing individual channels via a sync UI |
| 22 (P2-06) | **Rescan a camera from discovery** — `DeviceController::rescanDevice(id, user, password)` re-fetches a direct camera's ONVIF media profiles from the discovered device matching its address in the last scan, then reconciles its channel via `syncChannels` — refreshing stream URLs + re-syncing the stream profile while preserving the operator's channel name + stable id. Refuses recorders / unmatched devices / no-usable-stream honestly. A "Rescan" pill (camera cards) in `DevicesView.qml` uses the Discover panel's credentials. No schema change. `--devices-selftest` + offscreen `--smoke-ms --devices-demo` | ✅ built & verified (dev box): `--devices-selftest` green at **73 checks** (rescan re-fetches + reconciles, keeps the single channel, preserves the operator name, refreshes the main URL, refuses a recorder). Offscreen smoke clean. All 9 CTest suites green. With this **P2-06's buildable scope is complete** (rename · remove-confirmation · attach/detach · rescan); per-channel detach/rename + the live ONVIF fetch remain |
| 21 (P2-06) | **Device attach/detach** — the device-level sibling of inc 17's channel disable. Schema **v8** adds `devices.disabled`. `DeviceRepo::setDeviceDisabled` detaches a device — rows, channels (ids + operator names), and credential preserved, but the default group is emptied (`reconcileGroup` returns an empty group when detached); re-attach restores it. `listDevices` carries `disabled`; `DeviceController::setDeviceDisabled` + model `disabled`; `pollHealth` skips detached devices. `DevicesView.qml`: Attach/Detach pill, dimmed card, "⏸ detached" badge. `--devices-selftest` + offscreen `--smoke-ms --devices-demo` | ✅ built & verified (dev box): `--devices-selftest` green at **68 checks** (attached group holds channels, detach sets disabled + empties group + preserves channels, poll skips detached, re-attach restores). Offscreen smoke migrated the DB → v8, no QML errors/warnings. All 9 CTest suites green (`dbtest` pinned to v8). **Remaining in P2-06:** rescan-from-discovery, per-channel detach/rename |
| 20 (P2-06) | **Device rename + destructive-action confirmation** — first slice of P2-06. `DeviceRepo::renameDevice` updates the device name + its default group's display name while preserving every association (channels, stable ids, operator names, group membership, credential); honest `NotFound` on a bad id. `DeviceController::renameDevice`; `DevicesView.qml` gains inline device rename + a two-step confirm gate on Remove (was one click). `--devices-selftest` + offscreen `--smoke-ms --devices-demo` | ✅ built & verified (dev box): `--devices-selftest` green at **62 checks** (renameDevice succeeds, name updated, channels + group membership preserved, group display-name refreshed, unknown-device honest error). Offscreen smoke loads the rename/confirm UI with no QML errors/warnings. All 9 CTest suites green. **Remaining in P2-06:** detach-vs-delete semantics, rescan-from-discovery, reduced-channel handling — deferred for an explicit scope decision |
| 19 (P2-13) | **Device-health live feed** — makes the increment-13 `HealthMonitor` self-updating (the P2-13/14 "live polling" remainder). An injectable `DeviceHealthProbe` (`src/workspace/HealthProbe.h`): `MakeTcpHealthProbe` does a bounded `QTcpSocket` connect to the device's media port (reachability); a fixture fake serves scripted outcomes. `DeviceController::pollHealth()` probes every device → `reportReachable` (reachable→reach Online; unreachable→reach Offline + exception; stream left Unknown, since a TCP handshake ≠ a decoding stream); `main` drives it on a `QTimer` (`--health-interval`, `--no-health-feed`; off under `--devices-demo`). `DevicesView.qml` shows a live/manual feed badge. `--devices-selftest` + offscreen `--smoke-ms --devices-demo` | ✅ built & verified (dev box): `--devices-selftest` green at **56 checks** (adds feed-available, Unknown-until-observed, reachable→Online, unreachable→Offline + needs-attention, recovery→Online). Offscreen smoke loads the feed badge with no QML errors/warnings. All 9 CTest suites green. **Remaining (live/hardware):** the live TCP probe against the real camera; a richer probe (ONVIF firmware poll, a media open for true stream health) + off-thread probing for large inventories |
| 18 (P2-05) | **Channel reorder + stream-profile sync** — finishes P2-05's verb set. Schema **v7** adds `cameras.sort_order` (forward-only additive; listing is `ORDER BY sort_order, id`). `DeviceRepo` gains `reorderChannels` (device-scoped positional write) + `setStreamProfile` (upsert into the v2 `stream_profiles` per `(camera_id, tier)`); `listChannels` LEFT JOINs the `main`-tier profile so each channel carries its synced resolution/codec. `DeviceController` exposes `moveChannel(up/down)` and `onboardDiscovered` now syncs the chosen ONVIF profiles' resolution/codec (extended pure `chooseStreams`). `DevicesView.qml` channel rows gain ▲/▼ reorder + a `1920×1080 H265` readout. `--devices-selftest` + offscreen `--smoke-ms --devices-demo` | ✅ built & verified (dev box): `--devices-selftest` green at **51 checks** (adds schema v7, initial order by id, move-down/move-up/at-top no-op, and the discovered channel's main profile synced 1920×1080 H264 from the fixture). Offscreen smoke migrated the pre-existing v6 DB → v7 with no QML errors/warnings. All 9 CTest suites green (`dbtest` pinned to v7). The full P2-05 verb set (add/update/**reorder**/rename/disable/remove + **stream-profile sync**) is now built & offscreen-verified; live ONVIF confirmation shares the inc 11/12/16 hardware block |
| 17 (P2-05) | **Per-channel management / sync UI** — completes P2-05's "disable/remove individual channels" remainder over the sync core from inc 10. Schema **v6** adds `cameras.disabled` (forward-only additive; a disabled channel stays in inventory but leaves the device's default group). `DeviceRepo` gains `listChannels` / `renameChannel` / `setChannelDisabled` / `removeChannel`, and `reconcileGroup` now derives group membership from the **enabled** camera rows. `DeviceController` carries a per-device `channels` list + rename/disable/remove invokables; `DevicesView.qml` gains an expandable "Channels (N)" panel per device (inline rename, enable/disable, remove). `--devices-selftest` + offscreen `--smoke-ms --devices-demo` | ✅ built & verified (dev box): `--devices-selftest` green at **46 checks** (adds: schema v6, channel model lists all 6, group holds 6 enabled, rename preserves the stable id, disable keeps it in inventory but drops it from the group 6→5, remove leaves inventory 6→5 & group→4, re-enable restores it). Offscreen smoke migrated the pre-existing v5 DB → v6 and loaded the channel panels with no QML errors/warnings. All 9 CTest suites green (`dbtest` pinned to v6). **Remaining:** channel reorder (needs an order column) and stream-profile sync from a discovery source |
| 16 (P2-01/03/14) | **ONVIF discovery → onboard UI** — wires the ONVIF cores (WS-Discovery inc 11 + device/media SOAP client inc 12) into the Devices tab as a discover→onboard flow. An injectable `DiscoverySource` (`src/onvif/`) fronts the live UDP+HTTP transports so the flow is offscreen-testable; `DeviceController` gains a `discoveredDevices` model + `startDiscovery()`/`onboardDiscovered()`, a **pure `chooseStreams`** mapping (largest H.264/H.265 profile→Main, a smaller→Sub, JPEG/URI-less ignored), stable `deviceIdFromHost`, and **duplicate detection** against the inventory (P2-03). One-click onboard fetches the device's profiles + RTSP URIs and onboards atomically via `DeviceRepo` (kind `camera`, credential only via the `SecretStore`). A "Discover on LAN" panel in `DevicesView.qml`; live source windowed, a fixture-backed fake for tests/demo. `--devices-selftest` + offscreen `--smoke-ms --devices-demo` | ✅ built & verified (dev box): `--devices-selftest` green at **36 checks** (adds: discovery available, `chooseStreams` main/sub/JPEG-ignored, `deviceIdFromHost`, scan lists 2 candidates, new-vs-onboarded flag, `onboardDiscovered` end-to-end → mapped id + one channel + vendor from ONVIF info + credential via the SecretStore under the broker ref, re-scan flags the onboarded device). Offscreen smoke seeds 6 devices + one discovery pass (2 candidates, one colliding→"onboarded", one new), **no QML errors/warnings**. All 9 CTest suites green. **Remaining (live/hardware):** the real UDP `Discover()` + HTTP `FetchDevice()` on the LAN against the test camera (the live `DiscoverySource` drops in unchanged); multi-channel ONVIF encoders + bulk/activation-state selection (P2-03) are later slices |
| 6c-2 (P1-03/N0) | Callers connect by `camera_id`. `vms_grid` gains `--broker-db`/`--provision`/`--camera-id`: it provisions a camera (device + credential-free URL templates + secret via `CredentialRepo`) and resolves each tier's Main/Sub stream through the `ConnectionBroker` (URL materialized in memory only, never on the run CLI). `vms_workspace` has no URL-with-password path yet (synthetic clips). `vms_grid --broker-selftest` | ✅ built & verified (dev box): `--broker-selftest` green — 5 checks (broker resolves the camera by id, percent-encoded credentialed URL materialized in memory, main/sub template selection, unknown-camera honest failure). Completes the broker gate's standalone scope. **Broker path confirmed against a live camera (2026-07-29):** `--govern --broker-db --provision --camera-id` resolved the camera by id, materialized the credentialed URL in memory, connected to the real device, and surfaced honest per-tile typed failure (`Unauthorized` with placeholder creds — the whole resolve→materialize→connect→auth chain proven live); hardware decode confirmation is one valid-credential run away |

**Outstanding data:** run `vms_hwprobe.exe` on the low-end **i5 4th-gen / 8GB / no-GPU** machine
(expected `h265Main: false`). That profile shapes the governor in increment 4.

## Toolchain (installed on dev box)

- **Visual Studio Build Tools 2026** (MSVC v14.50, x64) + bundled **CMake** + **Ninja**.
- **Qt 6.11.1**, MSVC 2022 64-bit → `C:\Qt\6.11.1\msvc2022_64`.
- **GStreamer 1.28.5**, MSVC x86_64 (runtime + devel) → `C:\Program Files\gstreamer\1.0\msvc_x86_64`.
  Build is pinned to this via `-DGSTREAMER_ROOT`. (A redundant 1.26.11 is nested inside; recommend
  uninstalling to remove the `giolibproxy.dll` warning.)

## Build

From a **"Developer PowerShell for VS 2026"**:

```powershell
cd C:\Users\zubair\vms\native
cmake -S . -B build -G Ninja `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64" `
  -DGSTREAMER_ROOT="C:/Program Files/gstreamer/1.0/msvc_x86_64"
cmake --build build
```

### Headless build (for automated verification)

Runs configure+build without an interactive VS shell (used to build-verify each increment):

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
"%CMAKE%" -S C:\Users\zubair\vms\native -B C:\Users\zubair\vms\native\build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64" -DGSTREAMER_ROOT="C:/Program Files/gstreamer/1.0/msvc_x86_64"
"%CMAKE%" --build C:\Users\zubair\vms\native\build
```

## Running the tools

```powershell
# 1) Hardware probe (standalone; links only system DLLs)
.\build\vms_hwprobe.exe

# 2) Shell (needs Qt + GStreamer on PATH, or run the bundled dist)
$env:PATH = "C:\Qt\6.11.1\msvc2022_64\bin;C:\Program Files\gstreamer\1.0\msvc_x86_64\bin;$env:PATH"
$env:QT_PLUGIN_PATH = "C:\Qt\6.11.1\msvc2022_64\plugins"
.\build\vms_shell.exe

# 3) RTSP spike (GStreamer on PATH; URL/credentials supplied at run time only, never stored)
$env:PATH = "C:\Program Files\gstreamer\1.0\msvc_x86_64\bin;$env:PATH"
$env:GST_PLUGIN_PATH = "C:\Program Files\gstreamer\1.0\msvc_x86_64\lib\gstreamer-1.0"
.\build\vms_spike.exe "rtsp://<user>:<pass-with-%40>@192.168.0.254:554/Streaming/Channels/102" --seconds 30

# 4a) Grid measurement (GStreamer on PATH). --count replicates one camera URL N times
#     so you can ramp N and watch per-tile fps + composited dropped-frame count climb.
$env:PATH = "C:\Program Files\gstreamer\1.0\msvc_x86_64\bin;$env:PATH"
$env:GST_PLUGIN_PATH = "C:\Program Files\gstreamer\1.0\msvc_x86_64\lib\gstreamer-1.0"
.\build\vms_grid.exe "rtsp://<user>:<pass-with-%40>@192.168.0.254:554/Streaming/Channels/102" --count 9 --seconds 30
# ...or pass several distinct URLs instead of --count for a true multi-camera grid.

# 4a-tp) Camera-free fan-out to find the PURE decode+composite ceiling of a machine
#     (no camera session cap, no single low-res sub-stream). Encodes one synthetic
#     clip up front, then fans out N decode-only branches with the sink's sync OFF,
#     so per-tile fps = max decode throughput and sum(fps)/25 ~= sustainable streams.
$env:PATH = "C:\Program Files\gstreamer\1.0\msvc_x86_64\bin;$env:PATH"
$env:GST_PLUGIN_PATH = "C:\Program Files\gstreamer\1.0\msvc_x86_64\lib\gstreamer-1.0"
.\build\vms_grid.exe --test-pattern --count 16 --codec h265 --srcw 1920 --srch 1080 --seconds 30
# Ramp --count (16, 36, 64...) until aggregate fps stops rising / dropped climbs.
# On machines with NO H.265 hardware decode (the low-end i5 tier), use --codec h264
# to measure the QuickSync/DXVA H.264 ceiling instead. --srcw/--srch set the encoded
# source resolution (1920x1080 ~ a main stream; 640x480 ~ a sub stream).

# 4b) Apply a capacity plan seeded from the live hardware probe (auto is the default).
.\build\vms_grid.exe --test-pattern --govern --profile auto --count 64 --codec h265 --seconds 20
# Reproducible diagnostic overrides remain available as --profile devbox|lowend.
# --govern intentionally rejects RTSP input until real per-camera main/sub URLs are wired.

# 4b-sweep) Apply CHANGING plans to live branches: every --sweep-interval seconds the
#   focus moves to the next tile, the stateful governor re-plans, and the grid is
#   reconfigured to the new plan (releases applied before acquisitions).
.\build\vms_grid.exe --test-pattern --govern --sweep --profile auto --count 64 --codec h265 --seconds 24 --sweep-interval 6
# Watch each "== sweep: focus -> tile N ==" print its per-tile transitions, then the
# grid resume decoding within budget. A tight profile shows more movement:
.\build\vms_grid.exe --test-pattern --govern --sweep --profile lowend --count 8 --codec h265 --seconds 20 --sweep-interval 4

# 4b-rtsp) The governor selects each tile's REAL stream by tier: Main -> main URL,
#   Sub/Thumb -> sub URL, Paused -> no session opened. Pass one --camera per camera
#   as "mainUrl;subUrl" (encode any '@' in the password as %40). URLs are never
#   printed; only a credential-free "stream selection: t0=main t1=sub ..." line.
.\build\vms_grid.exe --govern --camera "rtsp://u:p%40host:554/Streaming/Channels/101;rtsp://u:p%40host:554/Streaming/Channels/102" --count 9 --seconds 30
# Add --sweep to also re-plan (which stream each tile opens) as focus moves.
# --count replicates one camera; or pass several distinct --camera entries.

# P3-14 s1) Honest per-tile state, RENDERED (Qt Quick; needs Qt on PATH; NO
#   GStreamer/camera). A governed grid where each tile's border + status strip
#   show its real state; a focus sweep re-plans every --sweep-interval seconds.
$env:PATH = "C:\Qt\6.11.1\msvc2022_64\bin;$env:PATH"
$env:QT_PLUGIN_PATH = "C:\Qt\6.11.1\msvc2022_64\plugins"
.\build\vms_workspace.exe --count 25 --profile lowend --sweep-interval 4
# --profile devbox|lowend selects a named calibration profile (MEASUREMENTS.md).
# lowend at a high --count shows the honest mix: 1 main/live, the rest thumb/
# degraded, the overflow capacity-paused, plus an "over capacity" header warning.
# Headless build/logic check (no window, no display, no camera):
.\build\vms_workspace.exe --selftest --count 64 --profile devbox

# P3-14 s2b) Live GOVERNED VIDEO under the honest chrome (Qt + GStreamer on
#   PATH). The governed d3d11-composited grid is the video source: each tile is
#   a real per-tier hardware decode (main 1080p / sub 640x480 / thumb 320x240),
#   composited -> d3d11download -> appsink -> a scene-graph texture, with the
#   state chrome aligned cell-for-cell on top. Paused tiles are composited black.
$env:PATH = "C:\Qt\6.11.1\msvc2022_64\bin;C:\Program Files\gstreamer\1.0\msvc_x86_64\bin;$env:PATH"
$env:QT_PLUGIN_PATH = "C:\Qt\6.11.1\msvc2022_64\plugins"
$env:GST_PLUGIN_PATH = "C:\Program Files\gstreamer\1.0\msvc_x86_64\lib\gstreamer-1.0"
.\build\vms_workspace.exe --video --count 16 --profile lowend --sweep-interval 3
# Omitting --profile uses auto (the DEFAULT): the governor is seeded from the
# live hardware probe (this dev box -> RTX 3050 hardware H.265, plans 25 main +
# 39 sub of 64). devbox/lowend remain explicit calibration overrides: devbox
# keeps all tiles at main/live; lowend shows the honest mix (main + sub + thumb,
# all decoding real video) under the chrome. Every --sweep-interval seconds the
# focus advances and BOTH the chrome and the live video re-plan to the new tiers
# in lock-step (the MAIN tile moves across the grid). Large interval = static.
# NOTE: the binary now links GStreamer, so even --selftest needs the DLLs on PATH.

# inc 14) Onboarding UI — the Devices tab (Qt on PATH; no camera needed). Lists
#   onboarded devices with their honest health, an onboard form, and per-device
#   acknowledge / maintenance / remove. --devices-demo seeds a few devices with
#   scripted health so the surface is populated without a live camera.
$env:PATH = "C:\Qt\6.11.1\msvc2022_64\bin;C:\Program Files\gstreamer\1.0\msvc_x86_64\bin;$env:PATH"
$env:QT_PLUGIN_PATH = "C:\Qt\6.11.1\msvc2022_64\plugins"
$env:GST_PLUGIN_PATH = "C:\Program Files\gstreamer\1.0\msvc_x86_64\lib\gstreamer-1.0"
.\build\vms_workspace.exe --devices-demo
# Switch to the "Devices" tab. Use the "Discover on LAN (ONVIF)" panel to Scan
# and one-click onboard a found device — its channels come from the device's own
# media profiles (inc 16; --devices-demo uses a fixture source, a real run scans
# the LAN). Or onboard a direct IP camera, or pick a recorder kind (NVR/DVR/
# Hybrid) and give a channel count + RTSP URL templates with a {ch} placeholder
# to onboard all channels at once (inc 15). Acknowledge / put in maintenance /
# remove a seeded one. Expand a device's "Channels (N)" to rename / enable-
# disable / remove / reorder (▲▼) individual channels; a discovery-onboarded
# channel shows its synced resolution/codec (inc 17/18). A real (non-demo) run
# also polls device reachability live (inc 19; --health-interval S, --no-health-
# feed). Headless verification:
.\build\vms_workspace.exe --devices-selftest            # 73-check controller logic
$env:QT_QPA_PLATFORM = "offscreen"                       # QML bindings, no window
.\build\vms_workspace.exe --smoke-ms 1500 --devices-demo --no-optimize
```

Verified result on the dev box (RTX 3050): hardware `d3d12h265dec`, ~25 fps, ~0 dropped frames.

## Increment 4 — split at the governance gate

Increment 4 divides into an unblocked measurement half and a gated policy half:

**4a — grid measurement spike (`vms_grid`, built).** Decode N cameras into one composited D3D11
window and report per-tile decode fps, the decoder each tile selected, composited rendered/dropped
frames, and process CPU/RAM. This is measurement only — the same character as inc 1–3 — and applies
no priority/tier/degrade policy. Its purpose is to find the smooth ceiling per hardware tier, which
is the empirical input the governor design needs. Run it on both the dev box and the low-end laptop.

**4b — first governor pass (approved; in progress).** "Only decode what this machine can handle" is
the actual product feature. P3-03 scope was approved and logged on 2026-07-24. Built so far: a pure
two-budget decision core, deterministic tier assignment, a stateful session that re-plans on
working-set/focus changes with low/high-water hysteresis, conservative profile construction from
the live hardware probe, and camera-free decode-path integration. The named dev-box/low-end
profiles are explicit test overrides; `auto` is the default. Applying changing session plans to
active branches, real RTSP main/sub selection, and honest Qt/QML per-tile state remain before
P3-03 can be marked Built and Verified.

**Outstanding data:** run `vms_hwprobe.exe` (and now `vms_grid.exe`) on the low-end **i5 4th-gen /
8GB / no-GPU** machine. That profile is the constraint the governor must be shaped around.
