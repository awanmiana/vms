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
(6a + 6b), including a no-plaintext scan of the main + WAL + shm DB files. 6c (the standalone
`ConnectionBroker`; rewire callers off URL-with-password) remains.

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
| 6b (P1-03/N0) | Credential wiring + atomicity. A `CredentialRepo` (`CredentialRepo.h/.cpp`, in `vms_persist`) binds `device_credentials.credential_ref` ↔ `SecretStore`: SQLite holds only the opaque ref, the plaintext secret never enters any DB file, and provisioning writes the ref row + encrypted secret **atomically** (both or neither). `vms_credtest`/CTest | ✅ built & verified (dev box): `credential_selfcheck` now 45 checks (6a + **19 for 6b**) — round-trip, opaque-ref, stable ref across rotation, both atomicity paths (failed secret write rolls back the ref row; unknown-device FK violation writes no secret), atomic + idempotent remove, and a **no-plaintext scan of the main + WAL + shm DB files**. **6c** (standalone `ConnectionBroker`; rewire `vms_grid`/`vms_workspace` off URL-with-password) next |

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
