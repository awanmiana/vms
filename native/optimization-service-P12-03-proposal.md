# System optimization service + node-connectivity resilience — scope proposal

> Status: **Scope approved 2026-07-29 (Option A — in-process health sampler).**
> Building as native increment 8; the core (SystemHealthProbe + Optimizer +
> bandwidth budget + report + `vms_opttest`) is built & verified, the live in-app
> sampler wiring remains. The controlling authority is
> `../Development_plan.md`. Per the Approval Contract (§2–4), no product code for
> this gate may start until its scope is discussed, explicitly approved, and
> recorded in the Approval Log. This document is the *discuss* step; it decides
> nothing on its own. It mirrors the format of `governor-P3-03-proposal.md`,
> `persistence-P0-04-proposal.md`, and `credential-broker-P1-03-proposal.md`.
>
> Owner request (2026-07-29, paraphrased): make the optimization check a scheduled
> task / service so the software always knows when the system is about to reach a
> bottleneck; make the optimization parameters smart and robust — max/free CPU
> cores, max/free RAM, OS priorities and problem tasks/services, loopback + local +
> LAN + internet ports and connection stability; on a problem the system should
> **not crash but switch** to another IP/port (local/remote, over the network or
> internet) — **for this software's own multi-deployment / node / AI-agent
> connectivity, explicitly NOT for NVR/DVR/camera devices**; enforce the grid-tile
> capacity gate (disallow more tiles / more cameras than hardware and bandwidth can
> sustain) with honest degradation (reduce quality, increase quantity) and
> upgrade-back when the operator re-chooses quality.

## 0. What is already built (so this gate does not re-scope it)

A large part of the request is **already built and verified** as the P3-03 decode
governor and the P3-14 workspace — this gate must *extend* that, not duplicate it:

- **Tile capacity gate + no-crash degradation** — `Governor::assign` caps requested
  tiles to two budgets (decode-throughput + GPU memory) and degrades the least
  important tiles first; requesting 64 main tiles on a constrained profile caps and
  degrades instead of crashing (P3-03 acceptance #1, verified: 3.77 GB vs a 10.5 GB
  crash). `src/governor/Governor.h`.
- **Quality↔quantity with upgrade-back** — the two orthogonal axes are already
  operator-interactive: device priority (`setPriority`, P3-14 s5) and per-view media
  tier (`setDesiredTier`, P3-14 s7). Capping frees budget for others; re-raising the
  tier upgrades back within capacity, and `GovernorSession` hysteresis
  (low/high watermarks) prevents flapping.
- **Honest state** — `TileState` (live / degraded / paused-offscreen / paused-capacity)
  is unit-tested and rendered.

**What is NOT built, and is the actual subject of this gate:**

1. The governor's capacity is seeded **once at startup** from *static* hardware
   facts (`HardwareInputs`: `logicalCores`, `totalRamMb`, `videoMemoryMb`,
   `hasHardwareDecode` — `Governor.h:167`). Nothing samples **live headroom** (free
   cores, free RAM, current GPU/decoder load, OS scheduling pressure) while running.
2. There is **no bandwidth budget** — decode and memory are modeled; the network
   link is not. "Do not open more cameras than the link can carry" is explicitly
   deferred in the P3-03 row.
3. There is **no scheduled/background trigger** — the governor re-plans only when the
   working set or focus changes, never *proactively* because a bottleneck is
   approaching.
4. There is **no node-connectivity / endpoint-health layer** and no failover for the
   software's own multi-deployment / AI-agent links.
5. There is **no optimization report** (P0-01F names one; it does not exist).

## 1. The decisions this gate actually makes

1. **Whether "optimization" is an in-process periodic sampler or a real OS-level
   service/scheduled task** that runs even when the UI is closed. (§4 Option A vs B.)
2. **What "about to reach a bottleneck" concretely means** — which live signals are
   sampled, at what cadence, and what thresholds trigger a proactive governor re-plan
   vs. only a warning. This must not become a busy-poll that itself becomes the
   bottleneck.
3. **Whether bandwidth becomes a third governor budget now** (admission control uses
   it exactly like decode/memory) or stays a warning-only signal for a first pass.
4. **What node-connectivity failover means for a standalone runtime that today has no
   peers** — and therefore how much of it is genuinely this gate vs. deferred to the
   coordinated gate (P1-10) that first defines what a "node" is.

## 2. Roadmap alignment (this gate spans several NFR gates — scope it as a first pass)

| Piece of the request | Owning roadmap item(s) | Note |
| :-- | :-- | :-- |
| Live capacity headroom → proactive governor re-plan | **P3-03** (extends it) + **P0-05/P0-06** (workstation/media capacity profiles) | The governor exists; this adds a live-headroom feedback loop |
| Scheduled optimization service + optimization report | **P0-01F** (names "capacity-aware optimization … and an optimization report") + **P12-03** (capacity & performance certification) | P0-01F is accepted at product-direction level; P12-03 is unapproved |
| Bandwidth-aware admission | **P3-03** (deferred bandwidth budget) + **P0-06/P0-09** | New third budget |
| OS priorities / problem services detection | **P0-05** + **P12-03** | New live signal |
| Node/endpoint connectivity + failover (this software's own links, not devices) | **P1-10** (coordinated) + **P0-03** (resilience/backoff/circuit-breaker) + **A0** (external-agent API) | Standalone has no peers yet — mostly a later gate |

Because this touches unapproved NFR gates (P0-05/06, P12-03) and the coordinated
gate (P1-10), the honest move is to approve a **narrow standalone-first first pass**
and leave the coordinated/OS-service pieces to their own gates.

## 3. Intended users & benefit

- **Administrators / operators** — the software stays smooth because it *sees a
  bottleneck coming* (rising decode load, shrinking RAM headroom, a saturating link)
  and degrades honestly **before** stutter/crash, then recovers quality when pressure
  clears — the "smoothness from intelligence, not brute force" principle
  (`ARCHITECTURE.md`) made continuous instead of event-driven.
- **Multi-node / AI-agent deployments (later)** — a peer/agent endpoint that degrades
  does not crash the software; it fails over to a healthy endpoint per the P0-03
  policy. (No standalone peers exist yet — see §6.)
- **Benefit:** honest capacity under *changing* runtime conditions (other apps
  competing for CPU/RAM, a congested network), not just the static startup snapshot;
  a visible optimization report; and resilient control-plane connectivity for the
  distributed profiles P0-01B/P0-01I describe.

## 4. The decision space (owner's call)

| Option | Shape | Fits | Cost |
| :-- | :-- | :-- | :-- |
| **A. In-process periodic sampler feeding the governor** | a low-cost timer samples live headroom (free cores, free RAM, GPU/decoder load, link estimate), converts it to an updated `CapacityProfile`, and asks `GovernorSession` to re-plan with hysteresis; an optimization report surface | standalone-first; extends existing code; "user installs nothing"; no privilege escalation | only runs while the app runs (no bottleneck watch when UI closed); OS-priority/problem-service detection is coarse from user space |
| **B. Real OS service / scheduled task** (Windows Service or Task Scheduler entry) | a background process watches system health even when the UI is closed and can adjust priorities / warn | matches the literal "scheduled task or service" request; always-on | needs install-time privilege, a service lifecycle, IPC to the app, and an uninstall story — conflicts with "user installs nothing" unless carefully bounded; larger security surface |
| **C. Node-health + failover layer** | endpoint registry + health probes + P0-03 backoff/circuit-breaker + switch-to-healthy-endpoint for the software's own links / AI-agent connectors | the coordinated / distributed profile and the A0 agent API | there is **no peer to fail over to** until P1-10 defines nodes; building failover now is speculative |

**Recommendation (for discussion, not a decision):** approve **A as increment 7a**
now (in-process sampler + live-headroom feedback into the existing governor + an
honest optimization report), and **bandwidth as a third governor budget as 7b**.
**Defer B** (OS service) until there is a demonstrated need to watch health with the
UI closed — and treat it as its own security-reviewed gate because it needs
privilege. **Defer C** (node failover) to **P1-10**, which must first define what a
node/peer/agent endpoint is; this gate can, at most, land the *interface*
(`EndpointHealth` + a failover policy hook) with no live peers, so P1-10 implements
it unchanged — the "same design, two deployments" pattern the broker already uses.

## 5. Included scope — first pass (7a + 7b), if the recommendation is approved

- **`SystemHealthProbe` interface (Qt-free, swappable, unit-testable)** with a Windows
  implementation and an injectable fake (like `SecretStore`/`Store`). Samples, per
  tick: total/available physical RAM (`GlobalMemoryStatusEx`), logical-core count and
  a **process + system CPU load** estimate (`GetSystemTimes` / `GetProcessTimes`
  deltas), the app's current **OS scheduling priority class**, and a coarse count of
  competing high-CPU processes (a *report* signal, never an automatic kill). GPU/
  decoder load is best-effort (from the existing probe / DXGI) and honestly reported
  as "unknown" when unavailable — per the P0-03 honesty rule.
- **A live-headroom → `CapacityProfile` mapping** that adjusts `decodeBudget` /
  `memoryBudgetMb` downward when free headroom shrinks and back up (bounded by the
  static ceiling) as it recovers, then calls `GovernorSession::update`. Re-uses the
  existing low/high-watermark hysteresis so the wall re-plans *before* exhaustion and
  does **not flap**. Purely feeds the governor — it never bypasses the two-budget
  admission logic.
- **A periodic scheduler** (in-process `QTimer` at a coarse, configurable cadence,
  e.g. 2–5 s) that owns the sample→map→re-plan loop, is itself budgeted (cheap
  syscalls only), and is off by default in `--selftest`.
- **Bandwidth budget (7b):** a third `TierCost.bandwidthKbps` and
  `CapacityProfile.bandwidthBudgetKbps`; admission rejects/downgrades tiles whose
  summed bitrate would exceed the estimated link budget, exactly as decode/memory do.
  First pass: budget is operator-configured and/or estimated from observed stream
  bitrates; true active probing is later.
- **Optimization report (honest, read-only):** a structured snapshot — current
  budgets, live headroom, what was degraded and *why* (decode vs memory vs bandwidth
  vs headroom), and the competing-process signal — surfaced in `vms_workspace` and
  dumped by a `--selftest`/CLI path. States are honest: "unknown" where a signal
  can't be read; it never claims headroom it didn't measure.
- **A0-ready:** the sampler's re-plan and the report are exposed behind a clean
  interface so they can later be a validated command / API read (P1-12/P1-13). No
  automatic OS-priority change or process kill in this pass — those are *report-only*
  recommendations pending a security-reviewed gate.
- **`vms_opttest` CTest self-check** (character of `vms_govtest`/`vms_dbtest`/
  `vms_credtest`): with an injected fake probe, assert that shrinking headroom
  degrades before exhaustion, that recovery upgrades back within the ceiling and does
  not flap, that a bandwidth-constrained plan admits fewer/lower tiles, and that the
  report attributes each degradation to the correct binding budget.

## 6. Explicitly deferred (out of this pass)

- **OS-level service / scheduled task that runs with the UI closed (Option B)** — its
  own gate; needs install privilege, service lifecycle, IPC, uninstall, and a
  security review. Not built here.
- **Automatic remediation of the OS** — changing process priority classes, killing/
  suspending competing services, or altering system settings is **prohibited as an
  automatic action**; the report *recommends*, a human acts. (Consistent with the
  safety rules on modifying system/security settings.)
- **Node-connectivity failover implementation (Option C)** — deferred to **P1-10**,
  which defines nodes/peers/agent endpoints. This pass may land only the
  `EndpointHealth` interface + failover-policy hook (no live peers), so P1-10
  implements it unchanged. Explicitly **excludes NVR/DVR/camera connectivity** — those
  stay behind the ConnectionBroker (increment 6) and P0-03; device failover is a
  different concern and is not in this gate at all.
- **True active bandwidth probing / QoS** and per-site WAN budgets — P0-06/P0-09/P12-03.
- **Numeric capacity certification** (the validated ceilings themselves) — **P12-03**;
  this gate builds the *mechanism*, not the certified numbers, which still need the
  low-end i5 calibration ramp that is already outstanding.

## 7. Measurable acceptance criteria (first pass)

1. **Proactive degrade:** with an injected probe whose free RAM / decode headroom
   falls below the high watermark, the governed wall degrades **before** the modeled
   wall is hit (no over-budget plan is ever emitted), and the transition is ordered
   release-before-acquire (re-uses `DiffPlans`).
2. **Recover without flapping:** when headroom recovers past the low watermark,
   quality upgrades back toward (never above) the static ceiling; a steady oscillating
   input at the boundary does not produce continuous re-planning.
3. **Bandwidth admission:** given a link budget smaller than the sum of requested tile
   bitrates, the plan admits fewer/lower tiles and the report attributes the
   limit to *bandwidth* (not decode/memory).
4. **Honest report:** every degraded tile in the report names its binding budget;
   any unreadable signal is "unknown", never fabricated.
5. **Bounded cost:** the sampler's own CPU cost over a fixed window is negligible
   (asserted against a budget) — the optimizer never becomes the bottleneck.
6. **No automatic OS mutation:** the self-check proves the service performs no
   priority change / process kill / system-setting write; those appear only as report
   recommendations.
7. **Boundary:** all live signals go through `SystemHealthProbe`; admission still goes
   only through the governor's two/three-budget logic; device connectivity is
   untouched (still the broker's job).

## 8. Alternatives considered

- **Keep capacity static at startup (today's behavior)** — rejected for the request:
  it cannot see a bottleneck *coming* from competing load or a congested link.
- **Build the OS service (Option B) first** — rejected for a first pass: privilege +
  lifecycle + security surface for a benefit (watch-while-closed) not yet demonstrated;
  the in-process sampler delivers the "sees the bottleneck coming" benefit while the
  app is running, which is when it matters for the wall.
- **Build node failover now (Option C)** — rejected: nothing to fail over to until
  P1-10 defines nodes; landing only the interface avoids speculative code.
- **Auto-tune the OS (raise our priority, kill competitors)** — rejected on safety
  grounds; recommend-only.

## 9. Proposed build sequence (native increment 7, each verified on hardware)

- **7a — `SystemHealthProbe` + live-headroom feedback + report + `vms_opttest`:** the
  interface, the Windows impl, the injectable fake, the sample→map→`GovernorSession`
  re-plan loop with hysteresis, the honest optimization report, and the CTest.
- **7b — bandwidth as a third budget:** `bandwidthKbps` in the cost model + profile,
  admission uses it, report attributes bandwidth-bound degradation; extend `vms_opttest`.
- **(deferred) 7c — `EndpointHealth` interface only:** the failover-policy hook with no
  live peers, so P1-10 implements it unchanged. Full node failover and the OS-service
  option are their own later gates.

## 10. Recorded Approval Log scope (draft — for the owner to approve/edit)

> System optimization service (standalone) | <date> | <decision> | First
> optimization pass, standalone runtime: a Qt-free swappable `SystemHealthProbe`
> (Windows impl + injectable fake) sampling live RAM/CPU/scheduling headroom and a
> coarse competing-process signal; an in-process periodic scheduler that maps live
> headroom to the governor's `CapacityProfile` and re-plans via `GovernorSession`
> with existing hysteresis (degrade before exhaustion, recover without flapping,
> admission still only through the two/three-budget logic); a bandwidth budget as a
> third admission budget; and an honest, read-only optimization report attributing
> each degradation to its binding budget. No automatic OS mutation (recommend-only);
> device/NVR/DVR/camera connectivity untouched (still the broker + P0-03).
> `vms_opttest`/CTest. Realizes the capacity-aware-optimization + optimization-report
> clause of P0-01F and the mechanism side of P0-05/P0-06 for the standalone runtime.
> Deferred: the OS-level always-on service/scheduled task (own security-reviewed
> gate), automatic OS remediation (prohibited/recommend-only), node-connectivity
> failover for multi-deployment / AI-agent links (P1-10; only the `EndpointHealth`
> interface may land here), true active bandwidth probing (P0-06/P0-09), and the
> certified numeric ceilings (P12-03, blocked on the low-end i5 calibration). Built
> behind a clean interface so it can later be a validated command/API read
> (A0/P1-12/P1-13). | Acceptance criteria §7 | Selects the in-process sampler
> (Option A) only; the OS service and node failover remain their own gates. |
> Building as native increment 7.
