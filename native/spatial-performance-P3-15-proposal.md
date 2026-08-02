# P3-15 spatial performance acceptance (native increment 32)

Date: 2026-08-01  
Status: built and verified on the dev box

## Problem and user

P3-15 has functional spatial behavior but no repeatable latency evidence. An
operator must be able to pan and zoom a 64-camera site without the viewport
policy or governor/model bridge consuming the 60 Hz frame budget, and an idle
viewport must not trigger hidden replans.

## Included

- A deterministic, headless `--spatial-benchmark [samples]` workload over the
  shipped `WorkspaceController`, real `GovernorSession`, and 64-tile model.
- Separate measurements for the no-change fast path and a deliberately
  expensive alternating-pan path that crosses zones and forces replans.
- Machine-readable CSV rows plus a human summary; non-zero exit when the
  regression limits or behavioral invariants fail.
- 60 Hz controller-budget targets: idle p95 <= 0.50 ms and replan p95 <= 4.00
  ms. The latter reserves at least 12.67 ms of a 16.67 ms frame for QML scene
  update, rendering, input, and OS scheduling.
- Confirmation that identical viewport passes emit zero replans and that the
  active workload actually exercises a replan for nearly every sample.
- A small scene-cost correction: do not paint FOV canvases for honestly culled
  (`paused-offscreen`) cameras.
- Offscreen 64-tile QML load/smoke evidence and recorded measurements in
  `native/MEASUREMENTS.md`.

## Explicitly excluded

- Live decoded video inside spatial tiles, compositor redesign, seamless media
  hot-swap, GPU/frame-time profilers, or a new map-tile architecture.
- Raising the product layout limit beyond 64 cameras, premature spatial-index
  structures, or changing viewport/governor semantics to win a benchmark.
- P3-18 analytics/operations or any premises administration expansion.

## Measurement contract

The gate uses p95 rather than a fragile single maximum. It reports maximum for
diagnosis but does not fail on one scheduler interruption. Debug and release
builds run the same limit on the approved dev box; slower deployment classes
must be measured separately before their capacity profile claims support.

## Acceptance

1. The 64-tile idle path emits no `planChanged` signal and meets 0.50 ms p95.
2. Alternating settled pans cross the canvas, force at least 95% of the expected
   replans, and meet 4.00 ms p95.
3. Output includes scenario, tile/sample/replan counts, mean/p95/max, limit, and
   PASS/FAIL; a failed invariant or limit returns non-zero.
4. A 64-tile offscreen spatial smoke loads without QML warnings/errors.
5. Existing spatial, command, persistence, coverage, native, and root tests stay
   green. The benchmark measures controller/policy/model cost; it does not claim
   decoded-video or physical-display frame latency.
