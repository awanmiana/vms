# VMS Native

Fully-native C++ VMS client. See [`ARCHITECTURE.md`](ARCHITECTURE.md) for the committed stack
and all decisions. The browser/Node code in the repository root is retained as a reference
specification, not the product runtime.

> ⚠️ Native C++ is authored here but compiled/run on your hardware. If a build surfaces
> compiler/linker errors, paste them and they get fixed.

## Current status (2026-07-23)

Foundation validated. Increments 1–3 build and run; self-contained packaging is proven. Increment
4a (`vms_grid`) is verified on a live camera (4× `d3d12h265dec` @ ~25 fps, ~0 ongoing drops, CPU ~0%)
and now has a camera-free `--test-pattern` fan-out to measure the pure decode ceiling per machine.
**Next up: run the fan-out ramp on both hardware tiers, then the gated governor pass (4b / P3-03).**

| # | Component | Status |
| :- | :- | :- |
| 1 | `vms_hwprobe` — D3D11 per-codec HW-decode probe + machine tier | ✅ verified (dev box) |
| 2 | `vms_shell` — Qt6 + GStreamer window (toolchain proof) | ✅ verified |
| 3 | `vms_spike` — RTSP → HW decode → D3D11 render + stats | ✅ verified (live camera) |
| — | Self-contained packaging (bundle Qt+GStreamer; user installs nothing) | ✅ proven (ran with cleaned PATH) |
| 4a | `vms_grid` — N streams → per-tile HW decode → `d3d11compositor` → one window; per-tile fps + composited rendered/dropped + process CPU/RAM; camera-free `--test-pattern` fan-out | ✅ verified on live camera (4-tile); fan-out ramp + low-end run pending |
| 4b | First governor pass (priority/tier policy, decode caps, degrade/recover) | ⛔ gated — P3-03 (unapproved); needs scope + Approval Log entry first |

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
```

Verified result on the dev box (RTX 3050): hardware `d3d12h265dec`, ~25 fps, ~0 dropped frames.

## Increment 4 — split at the governance gate

Increment 4 divides into an unblocked measurement half and a gated policy half:

**4a — grid measurement spike (`vms_grid`, built).** Decode N cameras into one composited D3D11
window and report per-tile decode fps, the decoder each tile selected, composited rendered/dropped
frames, and process CPU/RAM. This is measurement only — the same character as inc 1–3 — and applies
no priority/tier/degrade policy. Its purpose is to find the smooth ceiling per hardware tier, which
is the empirical input the governor design needs. Run it on both the dev box and the low-end laptop.

**4b — first governor pass (gated).** "Only decode what this machine can handle" — device priority
(high/med/low/idle) + per-view tier (main/sub/thumb/paused), concurrent-decode caps, and
degrade/recover with hysteresis — is the actual product feature. It maps to **P3-03** in
`../Development_plan.md` (`[ ] Discussed | [ ] Approved`) and leans on **P0-01L** (whose transitions
"will be discussed before this feature is implemented") and **P0-05**. Per the Approval Contract,
no governor product code starts until P3-03 is Discussed + Approved and logged. The `vms_grid`
numbers are meant to inform exactly that discussion.

**Outstanding data:** run `vms_hwprobe.exe` (and now `vms_grid.exe`) on the low-end **i5 4th-gen /
8GB / no-GPU** machine. That profile is the constraint the governor must be shaped around.
