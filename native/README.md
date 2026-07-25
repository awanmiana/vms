# VMS Native

Fully-native C++ VMS client. See [`ARCHITECTURE.md`](ARCHITECTURE.md) for the committed stack
and all decisions. The browser/Node code in the repository root is retained as a reference
specification, not the product runtime.

> ⚠️ Native C++ is authored here but compiled/run on your hardware. If a build surfaces
> compiler/linker errors, paste them and they get fixed.

## Current status (2026-07-24)

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
shown live (`tile 0 [main/live]`, `tile 5 [thumb/degraded]`); its *visual* rendering is the P3-14
Qt/QML gate. **Next up: the P3-14 visual state UI, a seamless (non-reloading) live apply, and (blocked
on hardware) live-camera RTSP confirmation and low-end i5 calibration.**

| # | Component | Status |
| :- | :- | :- |
| 1 | `vms_hwprobe` — D3D11 per-codec HW-decode probe + machine tier | ✅ verified (dev box) |
| 2 | `vms_shell` — Qt6 + GStreamer window (toolchain proof) | ✅ verified |
| 3 | `vms_spike` — RTSP → HW decode → D3D11 render + stats | ✅ verified (live camera) |
| — | Self-contained packaging (bundle Qt+GStreamer; user installs nothing) | ✅ proven (ran with cleaned PATH) |
| 4a | `vms_grid` — N streams → per-tile HW decode → `d3d11compositor` → one window; per-tile fps + composited rendered/dropped + process CPU/RAM; camera-free `--test-pattern` fan-out | ✅ verified on live camera (4-tile); fan-out ramp + low-end run pending |
| 4b | Decode governor — `vms_governor` decision core + stateful session, `vms_govtest`/CTest, and `vms_grid --govern` integration | 🟡 in progress; policy core passes at MSVC `/W4`; add/remove/focus re-planning and low/high-water hysteresis pass; **auto-probed decode integration verified on hardware: 64 requested main tiles become 1 main + 47 sub + 16 thumb and the corrected I420 path runs at 3.77 GB (`d3d12h265dec`) instead of crashing at 10.5 GB.** Changing plans are now **applied to live branches** via `vms_grid --sweep` (whole-grid NULL→rebuild on each plan change; seamless per-tile hot-swap deferred — it does not survive the D3D12 decoder + reused compositor pad). The governor's tiers also **select real RTSP main/sub streams** (`--govern --camera "main;sub"`; plumbing verified locally, live-camera decode pending). The cost model was **recalibrated** to the corrected I420 `d3d12h265dec` ramp (budget 15→64, sub cost 0.238→0.28, VRAM-tiered auto seed; auto now plans 25 main + 39 sub of 64, CTest green). The **honest per-tile state model** (live/degraded/paused-offscreen/paused-capacity) is built, unit-tested, and shown live. P3-03 scope approved 2026-07-24. Remaining: P3-14 visual state UI, seamless apply, and (hardware-blocked) live-camera RTSP confirmation + low-end i5 calibration |

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
