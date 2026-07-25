# P3-03 Decode Governor - Scope Proposal (increment 4b)

> Status: **APPROVED 2026-07-24; implementation in progress.** This is the
> "discuss before product code" artifact required by the Approval Contract.
> `../Development_plan.md` records P3-03 as Discussed + Approved. Numbers cited
> are from [`MEASUREMENTS.md`](MEASUREMENTS.md).

## 1. Problem

Naively decoding every camera tile overloads a machine, and the failure mode differs by
hardware. Measured on the dev box:

- Hardware decode (H.265): total decode is GPU-capped (~15 main / ~63 sub 25fps streams)
  and CPU is idle (~1%). **Memory is the hard wall** - RSS grew to 10.5 GB at 49 main
  tiles and **64 tiles crashed during setup** (window never opened).
- Software fallback (H.264, `avdec_h264`): the bottleneck moves to **CPU** (~88%).

So there is no single "max cameras" number: it depends on tier (main vs sub), codec, and
whether the machine has a hardware decoder. Brute force crashes; a fixed cap is wrong
across tiers and hardware.

## 2. Intended users

Operators viewing multi-camera grids on hardware ranging from a discrete-GPU workstation
to a 4th-gen i5 / 8 GB / no-GPU laptop; administrators provisioning those workstations.

## 3. Benefit

"Smoothness from intelligence, not brute force" (ARCHITECTURE.md): the client only decodes
what the machine can actually sustain, degrades honestly instead of freezing or crashing,
and behaves predictably across hardware tiers. Zero lag *within* auto-detected capacity.

## 4. The capacity model (core of this pass)

The governor tracks **two independent budgets** per machine, because the measurements show
two independent walls:

1. **Decode-throughput budget** - seeded from `vms_hwprobe` per-codec HW capability:
   - HW-decode path: a GPU decode ceiling (calibrated cost per tier from MEASUREMENTS.md).
   - SW-decode path: a CPU budget (software decode is CPU-bound).
2. **Memory budget** - system RAM / VRAM headroom, since memory is what actually crashes
   a HW-decode machine before it runs out of decode headroom.

Decode cost is modeled **per tier** (not a single pixel budget), because measurements show
real per-stream fixed overhead (main 1080p ~777 MP/s vs sub 640x480 ~482 MP/s).

## 5. Included scope (increment 4b)

- Startup capacity profile from `vms_hwprobe` + calibrated per-tier cost constants.
- Per-view tier assignment: **main / sub / thumb / paused**. The visible working set
  decodes at an appropriate tier; off-screen / paused tiles do not decode.
- **Admission control:** a tile decodes only if both budgets allow; otherwise it is
  assigned a cheaper tier or paused - never over-subscribed into a crash.
- **Degrade / recover with hysteresis:** approaching a wall, downgrade tiers in a
  deterministic order (main -> sub -> thumb -> paused); when headroom returns, upgrade,
  with hysteresis so tiers do not flap.
- **Honest per-tile state:** every tile shows its real state (decoding at tier X / paused
  / capacity-limited). A non-decoding tile never shows a stale frame as if it were live.
- **Priority hook (minimal):** device activity priority (high/med/low/idle) only decides
  which tiles keep the better tier under pressure. Full transition rules are out of scope
  (see 6).
- Validated in the `vms_grid` harness first (measurement rig we already have), then wired
  into the client decode path.

## 6. Excluded scope (deferred, with reasons)

- **Full high/med/low/idle transition rules**, alarm-driven priority elevation, fairness,
  per-host limits - P0-01L explicitly says these "will be discussed before this feature is
  implemented." This pass uses priority only as a tie-breaker under pressure.
- Real per-device stream-profile negotiation / actual sub-stream selection from live
  cameras (onboarding / P2-05 and the stream-selection half of P3-03 proper).
- Multi-monitor / video-wall budgets (P3-02 / P3-12).
- Network / bandwidth budget - this pass governs decode + memory only.
- Persisted governor configuration - awaits P0-04.
- The layered Qt/QML workspace UI itself (separate increment; P3-14).

## 7. Workflow

At startup: probe hardware -> build the capacity profile. As tiles open/close/scroll/focus,
the governor assigns each a tier within budget, decodes the working set, and continuously
monitors actual fps / memory / (CPU on the SW path), degrading and recovering. The operator
sees honest per-tile state plus an aggregate capacity meter.

## 8. Dependencies

- Built: `vms_hwprobe` (per-codec HW capability), `vms_grid` pipeline building blocks,
  dev-box cost constants in MEASUREMENTS.md.
- Pending calibration: true low-end i5 numbers (refine SW/CPU-path constants; not a design
  blocker).
- Roadmap: leans on P0-01L (priority model, partial) and feeds P0-05 (workstation/codec
  profile).

## 9. Data & security impact

Minimal. Governor config is local thresholds/caps. No new credentials, no new network
traffic, no PII. Reads local hardware info only. No persistence decision required now
(in-memory; optional local config later under P0-04).

## 10. Measurable acceptance criteria

1. **No crash:** requesting 64 main-stream tiles on the dev box does not crash; the
   governor caps concurrent main decodes and pushes the rest to sub/thumb/paused.
2. **Working set holds fps:** focused/working-set tiles hold their target fps (>=25) while
   capped.
3. **Under ceilings:** memory stays below a configured headroom margin; on the SW path CPU
   stays below its configured ceiling.
4. **No flapping:** at steady load, tier changes stay below a small rate (e.g. < N/min).
5. **Honest state:** every non-decoding tile shows a clear non-live indicator; no stale
   frame presented as live.
6. **Deterministic degrade order:** under pressure, tiles downgrade in the documented order.
7. **Both regimes:** caps on GPU+memory on a HW-decode machine; caps on CPU on a SW-decode
   machine. (Full SW-path validation needs the real low-end i5.)

## 11. Alternatives considered

- **No governor / brute force** - rejected: crashes (64 tiles, proven).
- **Single fixed cap (N streams)** - rejected: wrong across tiers (15 main vs 63 sub) and
  hardware (GPU- vs CPU-bound).
- **CPU/RAM heuristic (old browser prototype)** - rejected: CPU is ~1% on HW-decode
  machines, so CPU is the wrong signal there; must model decode throughput + memory.
- **Pure pixel budget** - rejected: measured per-stream fixed overhead means cost is
  per-tier calibrated, not pure pixels.

## 12. Recorded Approval Log scope

> P3-03 | 2026-07-24 | Approved (scope) | First decode-governor pass: two-budget capacity model
> (decode-throughput + memory) seeded from vms_hwprobe, per-tier cost (main/sub/thumb/
> paused), admission control, degrade/recover with hysteresis, honest per-tile state;
> priority used only as a pressure tie-breaker (full P0-01L transitions deferred) |
> Acceptance criteria section 10 | Governs decode+memory only; bandwidth, multi-monitor,
> persistence, and full priority model deferred.
