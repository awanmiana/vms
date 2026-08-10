# Broker-backed workspace live sources (P2-14 / P3-03 / P3-04 / P3-14)

Status: approved by the owner's 2026-08-10 direction to start the next build after the connectivity investigation. Native increment 48.

## Problem and intended outcome

The diagnostic `vms_grid` path can open real RTSP camera streams through `ConnectionBroker`, but the product `vms_workspace` grid still uses synthetic encoded clips. The workspace must consume inventory camera identities without exposing credentials, while preserving the governor's tier decisions and keeping one failed camera from taking down the wall.

## Included scope

- A portable `GridLiveSource` lease boundary between the media pipeline and connection providers.
- A `BrokerGridLiveSource` adapter mapping tile index → configured `camera_id` and governor tier → broker Main/Sub stream.
- Main → Main; Sub and Thumb → Sub; Paused acquires no session. A missing substream does not silently fall back to Main.
- A second `GridPipeline` construction mode using `uridecodebin` and the increment-47 RTSP transport policy.
- Immediate scrubbing of the credential-bearing URI after the GStreamer source property has copied it; no URI, username or password in console/diagnostic output.
- Broker session release on re-tier, layout rebuild, pipeline stop and source failure; success/failure feedback drives the existing circuit breaker.
- Per-branch unavailable state and credential-safe reason. Acquisition or RTSP failure is isolated instead of declaring the entire grid failed.
- `vms_workspace --live-camera <camera_id>` (repeatable) and `--rtsp-transport auto|tcp|udp|multicast`. Supplying a live camera enables video and requires the persistent inventory/secret store.
- Synthetic `--video` remains the default test/performance path and must not regress.

## Excluded and still evidence-gated

- No bundled credentials, live-device compatibility claim, automatic subnet scan or proprietary-port probing.
- No main-stream fallback for an unavailable substream, because it would invalidate governor capacity accounting.
- No RTSPS/SRTP trust decision, ONVIF replay, RTP/RTCP telemetry, audio/intercom or vendor SDK adapter.
- No claim that an authenticated RTSP stream decodes until tested against an authorized camera/NVR and firmware.

## Acceptance

1. Pure broker tests prove exact main/sub mapping, Paused/no-camera refusal, pool accounting, success/failure feedback, and lease URI scrubbing.
2. Workspace and grid compile with the live-source path; the existing synthetic path and all workspace gates remain green.
3. Unknown camera, missing secret and missing substream are typed failures and never result in a URL fallback.
4. No code path logs a credential-bearing URI.
5. Full native CTest, browser/reference regressions and `git diff --check` pass.

## Verification result (2026-08-10)

- Focused RTSP policy and broker self-checks pass, including exact tier mapping, pool accounting, feedback, and URI scrubbing.
- Offscreen live-source smoke with an unknown inventory camera exits 0, reports one explicitly unavailable branch, and emits no credential-bearing URI.
- The existing synthetic offscreen wall still reports 16/16 branches playing at 27.1 FPS and persists decoded streaming time.
- All 13 workspace gates, the full native build, aggregate native CTest 12/12 including ONVIF, browser/Node regressions, and whitespace validation pass.
- Authorized valid-credential camera/NVR decode and target low-end-machine performance remain evidence gates; they are not claimed by this increment.
