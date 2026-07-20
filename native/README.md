# VMS Native

Fully-native C++ VMS client. See [`ARCHITECTURE.md`](ARCHITECTURE.md) for the committed stack
and decisions. The browser/Node code in the repository root is retained as a reference
specification, not the product runtime.

> ⚠️ This code is written but **not compiled in the authoring environment** — you are the build/test
> harness on real hardware. If the first build surfaces compiler/linker errors, paste them and they
> get fixed immediately. Native C++ first builds commonly need small fix-ups; that is expected.

## Increment 1 — hardware probe (current)

A console program that detects the machine and prints a JSON profile: CPU, logical cores, RAM,
each GPU, and — most importantly — **per-codec hardware-decode capability** (H.264 / H.265) queried
from the Direct3D 11 video device, plus a coarse tier (Low / Typical / High). This is the input the
governor will use to tune the software to each machine.

No Qt or GStreamer needed yet — this deliberately isolates "does the toolchain build and run?" from
the video pipeline.

### Prerequisites

- **Visual Studio Build Tools 2026** (MSVC) — already installed.
- **Ninja** — already installed.
- **CMake ≥ 3.21** — not yet installed. Install one of:
  - `winget install Kitware.CMake`, or
  - the CMake bundled with VS Build Tools (under
    `...\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`), or
  - the standalone installer from cmake.org (add to PATH).

### Build & run

Open **"Developer PowerShell for VS 2026"** (it puts MSVC on PATH), then:

```powershell
cd C:\Users\zubair\vms\native
cmake -S . -B build -G Ninja
cmake --build build
.\build\vms_hwprobe.exe
```

### Expected output (shape)

On this dev machine (RTX 3050) you should see something like:

```json
{
  "cpuBrand": "...",
  "logicalCores": 12,
  "totalRamBytes": ...,
  "totalRamGB": ...,
  "tier": "High",
  "gpus": [
    {
      "description": "NVIDIA GeForce RTX 3050",
      "vendor": "NVIDIA",
      "dedicatedVideoMemoryBytes": ...,
      "isSoftwareAdapter": false,
      "decode": { "h264": true, "h265Main": true, "h265Main10": true }
    }
  ]
}
```

**The important test is the low-end target machine** (i5 4th-gen / 8GB / no GPU). There we expect
roughly: `tier: "Low"`, an Intel iGPU with `h264: true` but `h265Main: false` — proving in numbers
why that machine must prefer H.264 sub-streams and cap concurrent decodes. Please run it there and
share the JSON; those values calibrate the governor.

## Next increments (not built yet)

2. **Qt shell** — minimal Qt6 window (proves Qt + MSVC + CMake + Ninja).
   - Prep install: Qt 6 (online installer, MSVC 2022/2026 64-bit component).
3. **GStreamer decode+render spike** — one RTSP stream → D3D11 hardware decode → rendered in the
   Qt/QML view, with glass-to-glass latency, dropped-frame, and CPU/GPU instrumentation; then scale
   tiles to find the smooth ceiling per tier.
   - Prep install: GStreamer 1.x for **MSVC 64-bit**, both the **runtime** and **development**
     packages, from gstreamer.freedesktop.org.

Installing Qt and GStreamer now (while increment 1 is validated) means increment 2/3 can start
immediately.
