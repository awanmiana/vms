# Recording backend + footage retrieval for Playback — scope proposal

> Status: **Scope approved 2026-07-29 (Option A — optional local recording as the
> first footage source).** Building as native increment 7 (7a → 7b → 7c). Controlling authority:
> `../Development_plan.md`. Per the Approval Contract (§2–4), no product code for
> this gate starts until its scope is discussed, explicitly approved, and recorded
> in the Approval Log. This is the *discuss* step. Mirrors the format of
> `governor-P3-03-proposal.md`, `persistence-P0-04-proposal.md`,
> `credential-broker-P1-03-proposal.md`, and `optimization-service-P12-03-proposal.md`.
>
> Owner sequencing (2026-07-29): build recording + playback **before** the
> optimization service (7a), because real live + playback load is what makes the
> bottleneck signals meaningful. So this gate takes the next native increment slot
> and the optimization service is resequenced to follow it.

## 0. The constraint that shapes everything: P0-01E

The accepted architecture (P0-01E, and the Data/Media Ownership table) is explicit:
**the VMS must not create a mandatory central 24/7 archive.** NVRs, DVRs, edge
devices, or deployer-selected storage remain authoritative for recordings; the VMS
opens live streams transiently and **requests recorded footage on demand** for
playback/review. This gate must honor that: it does not turn the VMS into a
mandatory recorder. What it *may* add for the standalone runtime is an **optional,
deployer-controlled local recording** to **user-selected storage** (explicitly
allowed by P4-03 "workstation destinations" and P0-01G "the deployer controls where
VMS data is stored"), plus the retrieval/availability path Playback needs — behind
an interface a real recorder/ONVIF-replay source implements later unchanged.

## 1. The decision this gate makes

1. **Where playback footage comes from in the standalone runtime, today.** There is
   no live NVR integration yet (ONVIF replay / vendor SDK retrieval are later gates,
   P2-05 / P0-14, and need a real device). So the only footage source we can build
   and verify on the dev box now is **optional local recording of the streams the
   broker already opens**, written to deployer-chosen local storage and indexed. The
   decision: adopt that as the *first* `FootageSource`, behind an interface, and defer
   recorder/ONVIF retrieval to its device gate.
2. **What "unblocks Playback" concretely means.** Today the P3-14 s8 Playback
   instance is a chrome-only scaffold "awaiting recorded footage (Phase 4)." This
   gate feeds it **real availability segments** and **real transport** (seek / speed /
   frame-step over recorded files), honoring the P5-02 honesty rules (never present
   requested time as recorded; distinguish available / missing / unknown / overlapping).

## 2. Intended users & benefit

- **Operators** get a Playback tab that shows *real* recorded footage with honest
  gaps, a draggable playhead that actually seeks, and speed/frame-step — instead of a
  scaffold. **Administrators** choose whether local recording is on and where it
  writes (no forced archive, no forced location).
- **Benefit:** closes the last "honest scaffold" in the workspace with real media,
  and — per the owner's sequencing — produces the real recording + playback load the
  optimization service (7a) needs to measure a true bottleneck.

## 3. Roadmap alignment (a narrow first pass across several Phase-4/5 gates)

| Piece | Owning item(s) | This pass |
| :-- | :-- | :-- |
| Optional local recording to user-selected storage + segment index | **P4-03** (destinations, index authority, retrieval) | in scope (standalone, optional) |
| Footage availability query (available / missing / unknown / overlap) | **P5-02** | in scope |
| Playback range/timeline/transport over recorded files | **P5-02** + part of **P5-01** (scrub/speed/frame-step, single-camera) | in scope (single-camera first) |
| Recorder / NVR / ONVIF-replay retrieval | P4-03 + **P2-05 / P0-14** | deferred (needs a real device) |
| Schedules, dual-stream profiles, CVR/SAN, main/aux routing, retention loops, cloud, storage-health forecasting | P4-01/02/04/05/06/07/08/09 | deferred to their own gates |
| Synchronized multi-camera playback, search, evidence export/chain-of-custody | P5-01 (multi), P5-03..07, P5-08/09 | deferred |

## 4. Decision space (owner's call)

| Option | Shape | Fits | Cost |
| :-- | :-- | :-- | :-- |
| **A. Optional local recording as the first FootageSource** | record broker-opened streams to segmented files on deployer-chosen storage; index in SQLite; Playback reads that index | standalone-first; verifiable on the dev box now; honest to P0-01E as *optional* local recording | writes video to local disk (bounded by a simple cap in this pass; full retention loops are P4-06) |
| **B. Wait for a real recorder/ONVIF-replay integration** | Playback pulls only from external recorders | matches "recorders own footage" purely | blocked — no device integration yet (P2-05), so Playback stays a scaffold indefinitely |
| **C. Import pre-recorded files only** | no recording; Playback plays operator-provided clips | trivial | doesn't exercise the real record→index→retrieve path or produce realistic load |

**Recommendation (for discussion):** **A**, scoped tightly — optional, off by default,
to a deployer-chosen path, with a simple size/time cap (full retention is P4-06) —
behind a `FootageSource` interface so option B (a recorder/ONVIF-replay source) is a
drop-in later. This is the only choice that both honors P0-01E (optional, local,
deployer-controlled — not a mandatory central archive) and is verifiable now.

## 5. Included scope — first pass (increments 7a–7c), if approved

- **7a — recording sink + segment index.** A `RecordingSink` that tees a broker-
  opened stream to segmented files (`splitmuxsink`, fixed-duration segments) under a
  deployer-set directory; a `segments` table in the SQLite store (`camera_id`,
  `start_utc`, `end_utc`, `path`, `codec`, `bytes`) written atomically per completed
  segment via the C0-03 store contract. Off by default; enabled per camera. A simple
  total-size / max-age cap trims oldest segments (a placeholder for P4-06). Qt-free
  core + `vms_rectest`/CTest (segment rows are written, an interrupted segment leaves
  the index honest, the cap trims oldest-first).
- **7b — footage availability query.** A `FootageIndex::query(camera_id, range)`
  returning ordered segments merged into availability spans with honest state:
  **available / missing (gap) / unknown / overlapping**, never presenting requested
  time as recorded (P5-02). Unit-tested against continuous, gapped, overlapping, and
  empty cases.
- **7c — Playback wired to real footage.** The `vms_workspace` Playback instance
  (P3-14 s8) binds the availability query to its timeline (real segments + honest
  gaps) and its transport to real playback of the recorded files via a GStreamer
  playback pipeline: play/pause, draggable playhead **seek**, speed, and frame-step,
  single-camera first. The requested-range bar shows requested vs available honestly.
- **Boundary / A0:** recording control and footage queries sit behind clean
  interfaces so they can later be validated commands / API reads (A0 / P1-12/P1-13).
  All device access still goes through the ConnectionBroker (increment 6); recording
  never bypasses it.

## 6. Explicitly deferred

- **Recorder / NVR / ONVIF-replay retrieval** (the "request footage from the device"
  path) — P4-03 + **P2-05 / P0-14**; needs a real device and profile negotiation.
- **Recording schedules** (continuous/event/command, calendars, pre/post buffers) —
  **P4-02**. This pass records while a camera's recording is simply on.
- **Dual-stream recording profiles / storage estimates** — P4-01.
- **CVR/SAN, main/aux storage routing/replication** — P4-04/05.
- **Retention loops, protected evidence, legal hold, deletion proof** — **P4-06**
  (this pass has only a crude size/age cap, clearly labeled a placeholder).
- **Cloud recording/backup, encryption, regions** — P4-07/08.
- **Storage-health forecasting / SLO alerting** — P4-09.
- **Synchronized multi-camera playback** — the rest of **P5-01**.
- **Search (standard/event/ATM-POS/VCA/face/ANPR/NL)** — P5-03..07.
- **Evidence export, watermark/hash/signature, chain of custody** — P5-08/09.

## 7. Measurable acceptance criteria (first pass)

1. **Record→index:** with recording enabled on a camera, completed segments appear in
   the `segments` table with correct camera/time/path; an interrupted run leaves no
   phantom segment and no claimed footage for time that was not written.
2. **Honest availability:** a query over a range spanning recorded + gap + recorded
   returns available spans and an explicit **missing** gap; overlapping segments are
   marked overlapping; an empty range is **unknown/none**, never "available".
3. **Requested ≠ recorded:** the Playback timeline never colors requested time as
   available without a segment backing it (P5-02).
4. **Real transport:** playhead drag seeks to the correct recorded position; play/
   pause, at least one non-1× speed, and frame-step work on a recorded file (single
   camera).
5. **Optional + deployer-controlled:** recording is off by default and writes only to
   the configured directory; disabling it stops writes; nothing is recorded centrally
   or to an undisclosed location (P0-01E / P0-01G).
6. **Cap works:** the size/age cap trims oldest segments and updates the index; no
   half-deleted segment is ever presented as available.
7. **Boundary:** recording uses broker-opened streams (no direct URL-with-password);
   the store contract (atomic writes, typed errors) owns the index.

## 8. Alternatives considered

- **Make the VMS a full central recorder** — rejected: violates P0-01E; scope
  explosion (CVR/SAN/retention/HA) with no standalone need yet.
- **Ship Playback against imported clips only** — rejected: doesn't build or verify
  the real record→index→retrieve path, and gives the optimization service no realistic
  recording load to measure.
- **Block on a real NVR integration** — rejected: leaves Playback a permanent
  scaffold; the `FootageSource` interface lets that integration slot in later.

## 9. Proposed build sequence (native increment 7, each verified on hardware)

- **7a** recording sink + `segments` index + cap + `vms_rectest`/CTest.
- **7b** footage-availability query (honest spans) + tests.
- **7c** `vms_workspace` Playback wired to real segments + real transport (seek/
  speed/frame-step, single camera). Live-camera recording confirmation shares the
  existing 4b/6c hardware/credential block; local synthetic-clip recording verifies
  the full path on the dev box without a camera.

## 10. Recorded Approval Log scope (draft — for the owner to approve/edit)

> Recording backend + footage retrieval for Playback (standalone) | <date> |
> <decision> | First recording/playback pass, standalone runtime: an OPTIONAL,
> off-by-default, deployer-located local recording of broker-opened streams to
> segmented files with a SQLite `segments` index (atomic per-segment writes, typed
> errors, a crude size/age cap standing in for P4-06); a `FootageSource` /
> availability query returning honest available/missing/unknown/overlapping spans
> (P5-02, never presenting requested time as recorded); and the `vms_workspace`
> Playback instance wired to those segments with real transport (seek/speed/frame-
> step, single camera). Honors P0-01E (no mandatory central archive; recorders/
> devices remain authoritative; recording is optional and deployer-controlled).
> `vms_rectest`/CTest. Deferred: recorder/NVR/ONVIF-replay retrieval (P4-03/P2-05/
> P0-14), schedules (P4-02), dual-stream profiles (P4-01), CVR/SAN + storage routing
> (P4-04/05), retention loops/legal hold (P4-06), cloud (P4-07/08), storage-health
> forecasting (P4-09), synchronized multi-camera playback (P5-01), search (P5-03..07),
> and evidence export/chain-of-custody (P5-08/09). Built behind clean interfaces so
> recording control + footage queries can later be validated commands/API reads
> (A0/P1-12/P1-13). | Acceptance criteria §7 | Selects optional local recording
> (Option A) as the first FootageSource; recorder/cloud sources remain their own
> gates. | Building as native increment 7 (the optimization service resequences to
> follow it).
