# VMS Native

Fully-native C++ VMS client. See [`ARCHITECTURE.md`](ARCHITECTURE.md) for the committed stack
and decisions. The browser/Node code in the repository root is retained as a reference
specification, not the product runtime.

> ⚠️ Native C++ is authored here but compiled/run on your hardware. If a build surfaces
> compiler/linker errors, paste them and they get fixed. (Increments 1 and 2 are verified building
> and running on the dev machine.)

## Toolchain (installed)

- **Visual Studio Build Tools 2026** (MSVC v14.50, x64) — compiler.
- **CMake** (bundled with VS Build Tools) + **Ninja**.
- **Qt 6.11.1**, MSVC 2022 64-bit → `C:\Qt\6.11.1\msvc2022_64`.
- **GStreamer 1.28.5**, MSVC x86_64 (runtime + devel) → `C:\Program Files\gstreamer\1.0\msvc_x86_64`.
  - Note: a redundant 1.26.11 install is nested inside; recommended to uninstall it. The build is
    pinned to 1.28.5 via `-DGSTREAMER_ROOT`, so it is unaffected either way.

## Build

From a **"Developer PowerShell for VS 2026"** (puts MSVC on PATH):

```powershell
cd C:\Users\zubair\vms\native
cmake -S . -B build -G Ninja `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64" `
  -DGSTREAMER_ROOT="C:/Program Files/gstreamer/1.0/msvc_x86_64"
cmake --build build
```

## Increment 1 — hardware probe ✅ (verified)

`build\vms_hwprobe.exe` — console tool printing a JSON machine profile: CPU, logical cores, RAM, each
GPU, and per-codec hardware-decode capability (H.264 / H.265) queried from the Direct3D 11 video
device, plus a coarse tier (Low / Typical / High). Foundation for per-machine tuning.

```powershell
.\build\vms_hwprobe.exe
```

**Still needed:** run this on the low-end target (i5 4th-gen / 8GB / no GPU). Expected `tier: "Low"`,
Intel iGPU with `h264: true` but `h265Main: false`. Share that JSON to calibrate the governor.

## Increment 2 — Qt + GStreamer shell ✅ (verified)

`build\vms_shell.exe` — a small Qt window showing Qt/GStreamer versions and the detected tier. Proves
the whole toolchain links and runs together. No video yet.

To launch the **window** (needs Qt + GStreamer DLLs/plugins on PATH):

```powershell
$env:PATH = "C:\Qt\6.11.1\msvc2022_64\bin;C:\Program Files\gstreamer\1.0\msvc_x86_64\bin;$env:PATH"
$env:QT_PLUGIN_PATH = "C:\Qt\6.11.1\msvc2022_64\plugins"
.\build\vms_shell.exe
```

(For a self-contained copy later: `C:\Qt\6.11.1\msvc2022_64\bin\windeployqt.exe build\vms_shell.exe`
copies the Qt runtime next to the exe; GStreamer DLLs still come from its `bin`.)

`--selftest` runs headless and exits immediately (used for build verification).

## Increment 3 — GStreamer decode + render spike (next)

One RTSP stream → `d3d11` hardware decode → rendered in a Qt/QML view, instrumented for glass-to-glass
latency, dropped frames, and CPU/GPU. Then scale tiles to find the smooth ceiling per hardware tier
(pass criteria in `../Development_plan.md`). This is the measurement that validates the stack against
your zero-latency target on real hardware.

Needs: a reachable RTSP camera/NVR URL to point at.
