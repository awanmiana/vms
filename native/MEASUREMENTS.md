# Native decode-ceiling measurements (increment 4a)

Recorded evidence for **how many streams each hardware tier can decode**. This is
measurement only — it applies no governor policy and checks no roadmap feature box.
Its purpose is to give the **P3-03 / P0-05** governor discussion real numbers instead
of guesses. See `README.md` (increment 4a) and `../Development_plan.md` (Native Client
Track) for context.

## How to collect

Build `vms_grid` first, then run the sweep from a normal PowerShell:

```powershell
cd C:\Users\zubair\vms\native\scripts

# Dev box (RTX 3050 — has H.265 hardware decode):
.\ramp-decode.ps1 -Label devbox -Codec h265

# Low-end i5 4th-gen / 8GB / no-GPU (no H.265 HW decode — measure H.264 instead):
.\ramp-decode.ps1 -Label lowend-i5 -Codec h264 -Counts 2,4,6,9,12,16
```

Every run appends a row to `results/decode-ceiling.csv` and saves the full console
log under `results/logs/`. Fill the tables below from that CSV.

> Sanity check before a full sweep: run one count on its own and confirm it prints a
> `SUMMARY(test-pattern): ... aggregate decode fps=NNN` line with a non-zero number.
> `--test-pattern` shows **color bars, not cameras** — that is correct; the number is
> what matters.

## Reading the result

- `agg_fps` = total frames/sec decoded across all tiles (sink sync off = max throughput).
- `~streams` = `agg_fps / 25` = how many realtime 25 fps streams the box sustains.
- The **ceiling** is the tile count where `agg_fps` plateaus (and dropped frames / CPU
  climb). Beyond it, adding tiles buys no more real throughput — that is the number the
  governor must cap decoding at for this tier.

## Corrected dev-box ramp (I420, `d3d12h265dec`) — 2026-07-25

**This supersedes the 2026-07-24 tables below** (which predate the I420 fix and actually
ran on the slower `nvh265dec`/software paths). Rerun via `vms_grid --test-pattern --codec
h265 --srcw W --srch H --count N` on the corrected harness. Sink sync off = max throughput.

### Main stream (1920×1080)

| tiles | agg_fps | ~streams | peak RSS | decoder |
| ----: | ------: | -------: | -------: | :------ |
| 4     | 1771    | 71       | 468 MB   | d3d12h265dec |
| 9     | 1768    | 71       | 705 MB   | d3d12h265dec |
| 16    | 1775    | 71       | 985 MB   | d3d12h265dec |
| 25    | 1754    | 70       | 1313 MB  | d3d12h265dec |
| 36    | 1754    | 70       | 1760 MB  | d3d12h265dec |
| 49    | 1749    | 70       | 2247 MB  | d3d12h265dec |
| 64    | 1722    | 69       | 2750 MB  | d3d12h265dec |

### Sub stream (640×480)

| tiles | agg_fps | ~streams | peak RSS | decoder |
| ----: | ------: | -------: | -------: | :------ |
| 4     | 6228    | 249      | 598 MB   | d3d12h265dec |
| 9     | 6218    | 249      | 824 MB   | d3d12h265dec |
| 16    | 6170    | 247      | 1259 MB  | d3d12h265dec |
| 25    | 6065    | 243      | 1766 MB  | d3d12h265dec |
| 36    | 5867    | 235      | 2305 MB  | d3d12h265dec |

### What the corrected numbers change

- **Decode ceiling, main 1080p: ~70 sustainable 25 fps streams (~1750 agg fps)** — flat from 4
  tiles up (GPU-decode-engine bound). ~4.7× the stale NVDEC figure (~15).
- **Decode ceiling, sub 640×480: ~249 streams (~6200 agg fps).** So sub decode costs
  **~64/249 ≈ 0.28 main-equivalents** (was modeled 0.238).
- **Memory is no longer the wall on this path.** Process RSS grows ~38 MB per main stream and
  **64 main decoders ran at 2.75 GB with no crash** — versus the old NVDEC path that hit 10.5 GB
  and crashed at 64. The "memory is the hard wall" conclusion was a pre-fix NVDEC artifact; the
  corrected `d3d12h265dec` path is **decode-throughput-bound**. (Decode surfaces live in VRAM,
  which process RSS does not capture; 64 main 1080p decoders nonetheless ran within the RTX
  3050's VRAM, so VRAM was not the wall at 64 either.)
- **Caveat on per-tier RSS:** sub measured *higher* marginal RSS than main (~53 vs ~38 MB/tile)
  because sub decodes ~3.5× more frames/sec under sync-off, inflating in-flight buffer memory.
  That is not physical resident memory, so the cost model uses resolution-monotonic per-tier
  memory (main ≥ sub ≥ thumb) as a safety net, not these churn-inflated slopes.

### Recalibration applied to the governor (Governor.h / Governor.cpp)

- `DevBoxProfile.decodeBudget`: 15 → **64** (verified concurrent main decoders; ceiling ~70).
- `CostModel`: `main{1.0, 50}`, `sub{0.28, 35}`, `thumb{0.10, 20}` (was `main{1.0,200}`,
  `sub{0.238,38}`, `thumb{0.08,15}`). Decode is now the binding constraint; memory is a safety net.
- `MakeCapacityProfile` hardware-decode seed raised (VRAM-tiered: floor 12 / entry 20 / mid 28 /
  strong 40) and no longer scaled by CPU cores — still seeded **below** the measured ceiling so an
  unramped machine degrades early. A per-GPU ramp (or `--profile devbox`) uses the measured 64.
- Effect: on this box `--govern --profile auto` for 64 requested tiles now plans **25 main + 39
  sub** (RSS ~3.0 GB, no crash) instead of the stale **1 main + 47 sub + 16 thumb**. `vms_govtest`
  (CTest) still passes with the new constants.

## Dev box (GPU RTX 3050; CPU/RAM per vms_hwprobe) — 2026-07-24  *(superseded — see corrected ramp above)*

Codec: H.265, source 1920x1080 (main-stream sized). `d3d12h265dec` hardware decode.

> Audit note (2026-07-24): these original ramps predate the explicit I420/4:2:0
> test-source fix and actually reported `nvh265dec` in their logs. With corrected
> camera-typical I420 input, `decodebin` selects the higher-ranked
> `d3d12h265dec`, and both throughput and memory behavior change materially.
> Retain the tables below as calibration for the recorded NVDEC path, not as the
> final ceiling of the corrected harness. A fresh corrected main/sub ramp is
> required. Until then the auto profile intentionally keeps the lower historical
> 15-main-equivalent ceiling as a conservative seed.

| tiles | agg_fps | ~streams | cpu% | rss_mb | notes |
| ----: | ------: | -------: | ---: | -----: | :---- |
| 4     | 371–384 | 15       | ~1   | 758    | throughput already saturated |
| 9     | 375     | 15       | 1    | 1557   | still ~375; no gain over 4 |
| 16    | 281     | 11       | 1    | 2646   | dips — past saturation, overhead |
| 25    | 305     | 12       | 1    | 4012   |       |
| 36    | 328     | 13       | 2    | 6171   |       |
| 49    | 338     | 14       | 1    | 10497  | 10.5 GB RSS |
| 64    | —       | —        | —    | —      | **failed to initialize (memory wall); window never opened** |

**Ceiling (H.265, main-stream 1080p): ~15 sustainable 25 fps streams (~375 agg fps).**
Total decode throughput is set by the GPU decode engine and is flat from 4 tiles up —
adding decoders past ~9 does not raise throughput, it fragments it and adds overhead.
CPU stays ~1% (decode is fully GPU-offloaded). **Memory is the hard wall, not CPU or
throughput:** RSS grows ~linearly (~160–210 MB per 1080p decoder) and 64 tiles crashed
during setup. The machine degrades on memory *before* it runs out of decode headroom.

### Dev box — sub-stream (640x480), H.265 — 2026-07-24

| tiles | agg_fps | ~streams | cpu% | rss_mb | notes |
| ----: | ------: | -------: | ---: | -----: | :---- |
| 4     | 1568    | 63       | 1    | 305    |       |
| 9     | 1558    | 62       | 0    | 463    |       |
| 16    | 1576    | 63       | 0    | 690    | flat throughput ceiling |
| 25    | 1519    | 61       | 1    | 998    |       |
| 36    | 1401    | 56       | 1    | 1356   | slight overhead creep |

**Ceiling (H.265, sub-stream 640x480): ~63 sustainable 25 fps streams (~1570 agg fps).**

### Dev box main vs sub summary

| tier | ~streams | mem per stream | vs main |
| :--- | -------: | -------------: | :------ |
| main 1080p | ~15 | ~200 MB | 1x |
| sub 640x480 | ~63 | ~38 MB  | ~4x more streams, ~5x less memory |

Same GPU decoder, ~4x the camera count and ~5x less memory on sub-streams. This is the
empirical basis for the main/sub/thumb tier system: a large grid must run non-focused
tiles on sub-streams. Note decode cost is not purely pixel count (main 1080p sustains
~777 MP/s, sub ~482 MP/s), so there is real per-stream/per-frame fixed overhead — the
governor should model a calibrated per-tier decode cost, not a single pixel budget.

## Historical software-fallback artifact (H.264, `avdec_h264`) — 2026-07-24

> Labeled `lowend-i5` at run time, but the numbers show this ran on the **dev box**, not
> the low-end laptop: 716 fps of 1080p software H.264 at 6% CPU is impossible on a 4-core
> Haswell i5. Every tile used `avdec_h264` (software).
>
> Audit correction: the pre-fix synthetic encoder was free to negotiate H.264 4:4:4,
> which the installed hardware decoders do not accept. That forced `decodebin` to
> `avdec_h264` despite the RTX 3050 reporting hardware H.264 support. The harness now
> forces camera-typical I420/4:2:0; the same four-tile check selects `d3d12h264dec`.
> Therefore this table is retained as historical fallback evidence only. It is **not**
> a valid hardware ceiling, a representative camera-main measurement, or low-end
> calibration, and must not seed the governor.

| tiles | agg_fps | ~streams | cpu% | rss_mb | notes |
| ----: | ------: | -------: | ---: | -----: | :---- |
| 2     | 716     | 29       | 6    | 572    | (cpu tick likely caught wind-down) |
| 4     | 658     | 26       | 47   | 1078   |       |
| 6     | 661     | 26       | 77   | 1575   |       |
| 9     | 681     | 27       | 83   | 2363   |       |
| 12    | 759     | 30       | 88   | 3096   | CPU near-saturated |
| 16    | 790     | 32       | 81   | 4146   |       |

**Limited finding:** when a stream format forces software decode, CPU becomes the pressure
signal and climbed to ~88% here, while the compatible hardware path stays near idle. The
corrected I420 harness must be rerun on the actual low-end machine to establish its H.264
hardware or software ceiling.

## STILL NEEDED — true low-end i5 4th-gen / 8GB / no-GPU

Run on the *actual* laptop (not the dev box):

```powershell
.\build\vms_hwprobe.exe                 # confirm h265Main:false and h264 HW capability
cd native\scripts
.\ramp-decode.ps1 -Label lowend-i5-REAL -Codec h264 -Counts 1,2,3,4,6,8
.\ramp-decode.ps1 -Label lowend-i5-REAL -Codec h264 -Srcw 640 -Srch 480 -Counts 2,4,6,9,12,16
```

Expected: far lower fps, CPU pinned near 100% at a handful of tiles. This is the hard
constraint the governor must be shaped around.

| tiles | agg_fps | ~streams | cpu% | rss_mb | notes |
| ----: | ------: | -------: | ---: | -----: | :---- |
|       |         |          |      |        |       |

## Governor validation (increment 4b) - 2026-07-24

`vms_grid --govern` applies the P3-03 governor's tier plan to the decode branches
(paused tiles get no decoder; sub/thumb tiles decode a lower-resolution source).
`--profile auto` (the default) now builds the profile from the live probe: per-codec
D3D support, registered GStreamer hardware-decoder availability, RAM, dedicated VRAM,
and logical cores. Static specifications are only a conservative seed; the result is
bounded by the recorded 15-main-equivalent / 6 GB dev-box ceilings.

Same dev box and 64-tile request count. The ungoverned column is the historical
pre-I420 failure; the governed column is the corrected current harness, so this is
a safety comparison rather than a same-decoder throughput A/B. The corrected
ungoverned 64-tile case was intentionally not repeated after the earlier 10.5 GB
failure.

| | ungoverned | `--govern` |
| :--- | :--- | :--- |
| tiling | 64 x 1080p main | 1 main + 47 sub + 16 thumb |
| peak RSS | ~10.5 GB -> **crash (window never opened)** | **3.77 GB, ran the corrected I420 path for 10 s** |
| decoder | — (died at setup) | `d3d12h265dec` (hardware) on all 64 |
| aggregate decode | — | 5158 fps (~206 stream-eq; max-throughput harness, not display fps) |

Acceptance criterion #1 (64 tiles must cap, not crash) is met end-to-end on hardware.
The current auto-probed run resolved the machine as High / H.265 hardware decode /
RTX 3050, seeded decode=15 main-equivalents and memory=6000 MB, assigned 1 main +
47 sub + 16 thumb, and completed with all 64 branches on `d3d12h265dec`. Its model
reported 2226 MB of per-stream cost; observed process RSS peaked at 3773 MB, still
below the 5400 MB high-water cap. The difference is process/compositor and
decoder-specific overhead, which the next corrected ramp must use to recalibrate
the cost model.

## Dynamic re-planning applied to live branches (increment 4b) - 2026-07-25

`vms_grid --sweep` closes the "apply changing plans to active media branches" step. Every
`--sweep-interval` seconds it moves the focused tile to the next cell, re-runs the stateful
`GovernorSession`, diffs the new plan against the applied one with `DiffPlans` (releases ordered
before acquisitions so a re-plan never transiently over-subscribes), and reconfigures the grid to
the new plan. Verified on the dev box (RTX 3050):

| profile / grid | per focus move | observed |
| :--- | :--- | :--- |
| `lowend` 4-tile (tight budget) | 2 transitions (focused→main, previous→sub) | all 4 tiles resume ~700 fps `d3d12h265dec`, 0 dropped, focused tile stays `main`, clean exit |
| `lowend` 8-tile | main↔sub swaps as focus moves | swapped tiles resume, RSS steady ~0.7 GB |
| `auto` 64-tile (dev-box) | 2 transitions (the single `main` tile follows focus) | RSS resets on each reload then re-climbs, peak ~3.9 GB (bounded, far below the 10.5 GB crash zone), clean exit |

Acceptance #6 (deterministic degrade order) and the no-crash / bounded-memory guarantees hold across
runtime plan changes, not just at startup. The self-check (`vms_govtest`) adds pure coverage for the
same behavior: a `DiffPlans` release-before-acquire ordering scenario and a 64-tile runtime focus
sweep that stays in budget, keeps the focused tile at `main`, never pauses a dev-box tile, and does
not flap on a repeated working set.

**Mechanism note (honest):** the live apply reconfigures by taking the whole graph to `NULL`,
rebuilding every branch's decode front at its new tier, and returning to `PLAYING`, so all branches
preroll together from zero — the same path the flawless startup uses. A seamless per-tile hot-swap
(rebuild one branch's front in the running pipeline) was implemented and rejected first: the D3D12
hardware decoder plus the reused `d3d11compositor` sink pad leave the rebuilt decoder wedged in async
`PAUSED` at 0 fps and stall teardown. Making the apply seamless (no whole-grid reload on a plan
change) is a deferred optimization, tracked in the roadmap.

## Tier-driven RTSP main/sub selection (increment 4b) - 2026-07-25

The governor's per-tile tier now selects a **real camera stream** instead of a synthetic clip:
`--govern --camera "mainUrl;subUrl"` maps Main → the main stream, Sub/Thumb → the sub stream, and
Paused → no session opened at all (an off-working-set camera holds no connection). It runs both
statically and, plumbed, under `--sweep` (re-planning which stream each tile opens). Verified locally
on the dev box against an unreachable host (no camera credentials required for the plumbing test):

- 6 replicated cameras, `lowend` profile → governor assigns 1 main + 5 sub and prints the
  credential-free `stream selection: t0=main t1=sub t2=sub t3=sub t4=sub t5=sub`.
- Each tile builds a `uridecodebin` on the stream its tier selected and, when the host refuses,
  drops **that tile only** ("Could not open resource…"), exactly as a dead camera should behave.
- URLs (which carry the password) are never printed — only the `main|sub|sub(thumb)|none` label.

What remains for this to be marked verified end-to-end: a run against the real test camera (with
credentials, supplied at run time only) confirming main-tier tiles decode the main stream and
sub/thumb tiles the sub stream. Camera **profile discovery/negotiation** (ONVIF) is deliberately
out of scope here — that is P2-05; this step only wires operator-provided URLs to the tier decision.

Still open: live-camera confirmation of the above, honest per-tile UI state, seamless live apply, and
the true low-end i5 calibration.

## Spatial viewport/controller latency (P3-15 increment 32) — 2026-08-01

`vms_workspace --spatial-benchmark` measures the shipped 64-tile controller
path synchronously: zone classification → `GovernorSession` update → QVariant
model rebuild → Qt signals. The deterministic wrapper supplies the desktop
runtime paths from a normal PowerShell:

```powershell
cd C:\Users\zubair\vms\native\scripts
.\spatial-performance.ps1 -Samples 2000
.\spatial-performance.ps1 -BuildDir ..\out\perf-release -Samples 5000
```

Targets are p95 <= 0.50 ms for an identical/no-change viewport and <= 4.00 ms
for a settled pan that crosses zones and forces the governor/model path. The
active limit reserves at least 12.67 ms of a 16.67 ms 60 Hz frame for QML,
rendering, input, and OS scheduling.

Dev box: RTX 3050 / auto H.265 hardware profile, Windows, Qt 6.11.1, MSVC 19.50.

| build / scenario | samples | replans | mean ms | p95 ms | max ms | limit | result |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: | :--- |
| Debug / idle no-change | 2,000 | 0 | 0.0025 | 0.0026 | 0.0107 | 0.50 | PASS |
| Debug / settled pan replan | 2,000 | 2,000 | 1.6079 | 1.9852 | 2.9955 | 4.00 | PASS |
| Release / idle no-change | 5,000 | 0 | 0.0006 | 0.0006 | 0.0010 | 0.50 | PASS |
| Release / settled pan replan | 5,000 | 5,000 | 0.1358 | 0.1995 | 0.3148 | 4.00 | PASS |

The matching Release offscreen scene test (`--count 64 --spatial`) loaded QML
in **105 ms**, ran cleanly for 1.2 s, and the viewport policy left 25/64 tiles
decoding under the initial view. FOV canvases are no longer painted for cameras
whose honest state is `paused-offscreen`.

These numbers close the controller-side performance acceptance only. They do
not claim physical-display frame latency, GPU scene-graph time, decoded video
inside spatial tiles, or low-end hardware performance; those require their own
instrumented media/display runs.

## What this feeds

Once both tiers are filled in, the two ceilings define the range the decode governor
(P3-03, increment 4b) must adapt across: the low-end number is the hard constraint the
policy is shaped around; the dev-box number is the headroom. That is the input the
P3-03 scope discussion is waiting on.
