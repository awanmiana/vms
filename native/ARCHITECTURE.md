# VMS Native Build — Architecture Decision Record

> Status: Approved direction (owner-approved 2026-07-20). This records the committed
> technical decisions for the fully-native VMS client. It is a reference contract; the
> controlling authority remains `../Development_plan.md`. The native-shell boundary and a
> matching Approval Log entry should be reflected there by the owner (draft offered).

## Decision: fully-native C++ client

The browser/Node prototype (`../app.js`, `../backend/`, `../index.html`, ...) is retained as a
**reference specification** for behavior and domain model. The delivered product is a
fully-native compiled application.

### Committed stack

| Layer | Choice | Rationale |
| :- | :- | :- |
| Language / core | **C++ (C++17)** | Direct access to CPU/RAM/GPU/NIC for detection and per-machine tuning. |
| Media / protocols | **GStreamer** | Domain-correct native pipeline for RTSP/RTP/ONVIF/WebRTC/HLS; pluggable hardware decoders (d3d11/QSV/NVDEC on Windows, VAAPI/NVDEC on Linux). |
| Video render | **Direct3D 11** (zero-copy decode→present via GStreamer `d3d11` elements) | Lowest latency on Windows; GStreamer keeps decoded surfaces on the GPU. |
| UI | **Qt / QML** | GPU-accelerated scene graph for the layered click-through overlay; multi-monitor / video-wall ready; cross-platform. |
| Local store | **SQLite** | Schema already drafted in `../docs/sqlite-schema.sql`. |
| Hardware detect | Win32 / DXGI / D3D11 (Linux equivalents later) | Feeds the tuning profile and the governor. |
| Credentials | **OS secret store** (Windows DPAPI now; libsecret/keyring on Linux later) | App stores only a `credential_ref`; secrets never in plaintext. |

### Why not raw Media Foundation + D3D11 (Windows-only)

- MF has no robust native RTSP ingestion — an RTSP/demux library (GStreamer/FFmpeg/live555) is
  required regardless, so "pure MF" is not a complete path.
- GStreamer's `d3d11` decoders use the **same GPU decode silicon** as MF; the glass-to-glass
  delta is ~0–30 ms out of ~100–250 ms (dominated by the jitter buffer + camera GOP, equal for both).
- MF+D3D11+Win32 is Windows-only; Linux would mean rewriting the whole media/render/UI layer.
- **Escape hatch:** if a specific path is ever proven latency-critical and GStreamer can't meet it,
  drop to raw MF+D3D11 for that one Windows path only.

Cross-platform intent: portable C++ core behind interfaces; GStreamer + Qt run on Windows and Linux,
so the media + UI layers are written once.

## Decision: centralized connection broker for device access

- Operators authenticate to the **VMS** (their own username/ID + password, role-based). They never
  receive device credentials and never connect to NVRs/DVRs directly.
- Only the **broker** connects to devices. It holds device credentials encrypted (DPAPI) and is the
  single connection identity to each device.
- Solves at the root: (1) **leaver cleanup** — disable the VMS account centrally; the person never
  held device credentials; (2) **device lockouts** — one connection identity + connection pooling
  (respect device max-session limits) + **login backoff / circuit-breaker** (the P0-03 resilience
  retry policy) so devices are never hammered into a lock.
- **Same design, two deployments:** standalone = broker runs in-process locally (DPAPI); coordinated
  (later gate) = shared server with a vault, RBAC, and audit. Identical operator/broker interface.

## The tuning principle (non-negotiable)

Smoothness comes from **intelligence, not brute force**. Detect the machine → adapt codec/tier/decoder
choice → cap concurrent decodes → degrade honestly. Only the on-screen working set decodes; the rest
are pins/thumbnails/paused. "Zero lag" = zero lag *within honestly auto-detected capacity*, because the
software refuses to overload itself. (Matches P0-03H / P0-01L / P3-03.)

## Two orthogonal control axes (kept separate)

- **Device activity priority**: high / medium / low / idle — is a device worth a connection/decode at all?
- **Per-view media tier**: main / sub / thumb / paused — quality of a visible tile.

## Build sequence (incremental, each verified on real hardware before the next)

1. **[in progress] Hardware probe (console, C++/Win32/DXGI/D3D11).** No Qt/GStreamer. Prove the toolchain;
   output a machine profile (CPU, RAM, GPUs, per-codec HW-decode capability, tier).
2. **Qt shell.** Minimal Qt6 window; prove Qt builds/runs with MSVC + CMake + Ninja.
3. **GStreamer decode+render spike.** One RTSP stream → `d3d11` HW decode → render in the Qt/QML view;
   measure glass-to-glass latency, dropped frames, CPU/GPU. Then scale tiles to find the smooth ceiling
   per hardware tier (pass criteria in `../Development_plan.md` spike targets).
4. Broker + SQLite + credential store; then the layered workspace + governor.

## Decision: fully self-contained package (no user-installed dependencies)

The end user installs **nothing** — no GStreamer, no Qt, no runtime. We **bundle** a pinned
GStreamer version's DLLs + the specific plugins we use, plus the Qt runtime (via `windeployqt`),
inside the application folder / installer. The app loads them from its own directory, so whatever
is (or isn't) installed on the user's machine is irrelevant — there is no version-matching problem
and no "wrapper" needed.

Proven 2026-07-20: a `vms_shell` copy with bundled Qt + GStreamer ran with GStreamer and Qt fully
removed from `PATH` (`PATH=%SystemRoot%\System32;%SystemRoot%`) and still reported both versions.

Packaging notes / TODO for the real installer:
- Build **Release** (the proof used a Debug build → debug Qt DLLs; Release is leaner and has no debug-CRT dependency).
- **Trim** the bundled GStreamer plugin set to only what the pipeline uses (the proof copied all `bin` DLLs).
- For D3D12 decode paths, bundle `dxcompiler.dll` / `dxil.dll` (windeployqt warned they were missing).
- Wrap the assembled folder in an installer (WiX/MSI, Inno Setup, or NSIS) — a routine later step.

## Codec-licensing gate (flagged, not resolved)

H.264/H.265 patent licensing for a distributed product is a P0-14/P0-17 gate. Preferring OS/GPU decoders
(not bundling codec libraries) generally sidesteps it. Do not distribute before this is cleared.
