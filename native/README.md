# VMS Native

Fully-native C++ VMS client. See [`ARCHITECTURE.md`](ARCHITECTURE.md) for the committed stack
and all decisions. The browser/Node code in the repository root is retained as a reference
specification, not the product runtime.

> ⚠️ Native C++ is authored here but compiled/run on your hardware. If a build surfaces
> compiler/linker errors, paste them and they get fixed.

## Current status (2026-07-22)

Foundation validated. Increments 1–3 build and run; self-contained packaging is proven. **Next up:
increment 4 — multi-tile grid + first governor pass.**

| # | Component | Status |
| :- | :- | :- |
| 1 | `vms_hwprobe` — D3D11 per-codec HW-decode probe + machine tier | ✅ verified (dev box) |
| 2 | `vms_shell` — Qt6 + GStreamer window (toolchain proof) | ✅ verified |
| 3 | `vms_spike` — RTSP → HW decode → D3D11 render + stats | ✅ verified (live camera) |
| — | Self-contained packaging (bundle Qt+GStreamer; user installs nothing) | ✅ proven (ran with cleaned PATH) |
| 4 | Multi-tile grid + first governor pass | ⏳ next |

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
```

Verified result on the dev box (RTX 3050): hardware `d3d12h265dec`, ~25 fps, ~0 dropped frames.

## Increment 4 — multi-tile grid + first governor pass (next)

Decode several cameras at once in a grid, read the machine profile from `vms_hwprobe`, and enforce
"only decode what this machine can handle" (device priority + per-view tier; cap concurrent decodes;
degrade/recover with hysteresis). This is where the adaptive-optimization vision becomes real code,
and it must be shaped by the low-end machine's profile.
