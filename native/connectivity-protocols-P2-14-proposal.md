# Native connectivity and protocol contract (P2-14 / P3-04)

Status: approved transport-foundation slice; native increment 47 built and verified on the development machine on 2026-08-10. Live-device and vendor-adapter claims remain evidence-gated.

## Decision

The native product uses a capability-negotiated adapter system. It does not infer a protocol from a port number, probe arbitrary proprietary services with credentials, or claim that a GStreamer plugin being installed makes that protocol a supported product integration.

ONVIF is the preferred surveillance discovery and management contract. RTSP is the primary live/replay session-control contract; RTP/RTCP carries its media using an explicitly selected lower transport. Exact vendor SDK, cloud/P2P, model, firmware, licence, and redistribution requirements remain separate adapter gates.

## RTSP: TCP and UDP are both correct

RTSP control is carried on a reliable TCP connection. The `SETUP` negotiation can carry RTP and RTCP media by:

1. RTP/RTCP over UDP unicast (separate negotiated ports; low latency on a healthy LAN).
2. RTP/RTCP over UDP multicast (efficient one-to-many delivery on a managed multicast network).
3. RTP/RTCP interleaved over the RTSP TCP connection (one firewall-friendly connection; packet loss can cause TCP head-of-line delay).

The implemented policy is `auto | tcp | udp | multicast`. `auto` follows the GStreamer/RFC-compatible order UDP unicast, UDP multicast, then TCP; a five-second UDP timeout permits TCP fallback. TCP connect/read is bounded at 20 seconds. RTCP remains enabled for liveness, synchronization, and future transport telemetry. The prior hidden TCP-only magic value is removed.

The live diagnostic tools accept:

```text
vms_spike <rtsp-uri> --rtsp-transport auto|tcp|udp|multicast
vms_grid  <rtsp-uri> --rtsp-transport auto|tcp|udp|multicast
```

The default is `auto`. A deployment may set `tcp` for NVRs behind firewalls/NAT, `udp` for a controlled low-latency LAN, or `multicast` only when IGMP, routing, address allocation, and switch behavior are managed. The selected policy is printed without exposing credentials.

## Port interpretation

- `554` is the usual RTSP port; `8554` is also registered as an alternate. The actual URI returned by ONVIF or configured by the device controls.
- Hikvision documents `8000` as its default **device port** used when adding a device to iVMS-4200. That is not evidence that video is RTSP on port 8000. The device can use that service connection while its media URI uses RTSP on another port and negotiates TCP or UDP.
- `34580` cannot be identified safely from the number alone. It is treated as an opaque vendor service endpoint until the exact manufacturer, model, firmware, official protocol/SDK contract, licence, and a packet-level test are available.

## Practical protocol and discovery matrix

| Family | Media transport/use | Discovery or signalling | Product status and rule |
| --- | --- | --- | --- |
| ONVIF Core + Media/Media2, Profiles T/G | WS-* device/media/replay control; returns RTSP media URIs | WS-Discovery, then Device service capabilities, profiles and `GetStreamUri`/`GetReplayUri` | Core discovery and Media onboarding are fixture-verified and wired to the UI; live NVR/camera proof and conformance matrix remain. Prefer current conformant products and fail closed on absent capabilities. |
| RTSP/RTSPS + RTP/RTCP/SRTP | Live and recorded surveillance; UDP unicast, UDP multicast, TCP interleaving, and advertised HTTP(S)/WebSocket traversal | ONVIF URI, SDP `DESCRIBE`, manual URI, or an approved vendor adapter | RTSP TCP/UDP/multicast policy is integrated in `vms_spike`, `vms_grid`, and the broker-backed production workspace. RTSPS/SRTP, RTSP-over-HTTP/WebSocket, certificate policy, ONVIF replay, RTCP metric extraction, and authorized live-device conformance remain separate verified slices. |
| RTP/MPEG-TS over UDP | Managed-LAN contribution/distribution, unicast or multicast | Static SDP, SAP/SDP, or a management registry | Runtime elements are installed; no product adapter yet. Require source-address, interface, multicast, payload, clock and timeout configuration. SAP is experimental and untrusted announcements must not auto-onboard. |
| HTTP(S) progressive/MJPEG | Simple cameras, snapshots, or multipart JPEG | ONVIF/vendor capability or configured URL | Runtime HTTP source exists; no product adapter. Enforce TLS validation, authentication, size/rate limits, and MIME/container validation. |
| HLS / MPEG-DASH | HTTP adaptive live/VOD distribution; normally more latency than direct surveillance RTSP | Configured manifest/playlist URL or service API | Runtime demuxers are installed; no product adapter. These are useful for remote distribution/playback, not LAN camera discovery. |
| WebRTC / WHIP | ICE + DTLS-SRTP + RTP/RTCP; low-latency browser/agent contribution | HTTPS SDP offer/answer endpoint plus ICE/STUN/TURN; WHIP is ingest | Runtime WebRTC and WHIP elements are installed; no product adapter. A receiving/egress API must be selected and verified independently; do not label an evolving/non-standard endpoint as WHEP without its exact contract. |
| SRT | Reliable encrypted UDP contribution over lossy/WAN links; caller/listener/rendezvous | Configured endpoint/stream ID; no universal camera discovery | Runtime SRT source is installed; no product adapter. Require encryption policy, latency, payload/container, mode, failover and statistics contracts. |
| RIST | Reliable interoperable contribution over lossy networks | Configured endpoint; RIST TR-06-4 Part 5 defines multicast discovery | Runtime RIST source is installed; no product adapter. Select Simple/Main/Advanced Profile and encryption/authentication requirements explicitly. |
| RTMP/RTMPS | Legacy live contribution/distribution, commonly configured service URLs | Service/vendor API or manual publish/play URI | Runtime RTMP source is installed; no product adapter. Treat as legacy; require TLS where supported and do not use port scanning as discovery. |
| NDI and other licensed LAN media SDKs | Vendor-defined low-latency IP media | SDK discovery, commonly assisted by mDNS/DNS-SD | Not implemented. Treat as a licensed proprietary adapter with exact SDK/version, redistribution, CPU/GPU and multicast/network evidence; do not equate an mDNS advertisement with authorized support. |
| SMPTE ST 2110 / ST 2022 | Professional managed-IP RTP essence, multicast, precise timing/redundancy | SDP/SAP or AMWA NMOS IS-04 DNS-SD/registry; IS-05 connection management | Not a surveillance-default adapter. It requires PTP, multicast engineering, NIC/switch capacity, NMOS authorization and dedicated lab evidence. |
| GB/T 28181 | Regional surveillance interconnect using SIP signalling and RTP-based media | SIP registration/catalogue/query | Not implemented. Add only for a confirmed deployment requiring GB/T 28181-2022 and verify national-profile, codec, security, NAT and conformance requirements. |
| PSIA IP Media / Recording and Content Management | Legacy camera, DVR/NVR/VMS interoperability | PSIA service/resource contracts or configured endpoint | Not implemented; legacy adapter only when a named device requires it. Do not silently fall back from ONVIF. |
| Proprietary NVR/DVR SDK, cloud/P2P and vendor service ports | Vendor-defined control, playback, events and sometimes media | Vendor SDK/API, provisioning service, or documented discovery | Open-ended by definition. One sandboxed adapter per approved vendor/version, with licensing, secure secret handling, timeout/cancellation, capability manifest, telemetry and compatibility evidence. No unsupported reverse engineering claim. |

UPnP/SSDP and generic mDNS/DNS-SD may locate a device or advertised service, but neither proves that it is a camera nor authorizes onboarding. They are auxiliary candidate sources only. Subnet probing is separately scoped, rate-limited, explicitly authorized, and never sends credentials until the operator selects a candidate and its adapter contract is known.

Codecs and containers are negotiated separately from transport. H.264/H.265/JPEG/AAC/G.711/G.726, MPEG-TS, fragmented MP4 and other formats are supported only when the device advertises them, the installed decoder/parser accepts the exact profile/level/payload, licensing permits shipment, and a compatibility test passes. Unknown formats fail with a typed unsupported-capability result.

## Runtime capability audit on this development machine

The installed MSVC GStreamer runtime exposes `rtspsrc`, `rtpbin`, `souphttpsrc`, `hlsdemux`, `dashdemux`, `webrtcbin`, `whipclientsink`, `srtsrc`, `ristsrc`, `rtmpsrc`, `udpsrc`, `tcpclientsrc`, `sdpdemux`, and `onvifmetadataparse`. This is build/runtime availability only. Product support requires adapter wiring, security policy, cancellation/timeout behavior, metrics, tests and target-device evidence.

## Integration order and acceptance boundary

1. Complete ONVIF live discovery/fetch/onboarding evidence across authorized camera and NVR models; the production workspace→`ConnectionBroker` identity binding itself is complete in increment 48.
2. Persist per-profile RTSP policy and apply it to live and ONVIF replay pipelines; add bounded reconnect/backoff and expose the negotiated lower transport.
3. Extract RTP/RTCP bitrate, jitter, loss, round-trip and clock evidence without inventing zero values when unavailable.
4. Add RTSPS/SRTP and certificate/trust configuration against the current ONVIF security baseline.
5. Add protocols only in deployment order: regional surveillance (if required), WAN contribution (SRT/RIST), HTTP adaptive/WebRTC, then professional broadcast (NMOS/ST 2110). Each adapter gets fixture, loopback/fault, live-device, packaging and compatibility-matrix gates.

This slice does not claim live credentials, remote/NVR replay, low-end performance, TLS/SRTP, proprietary SDK compatibility, or delivery-provider completion.

## Standards baseline

- IETF RFC 7826 (RTSP 2.0), RFC 3550 (RTP/RTCP), RFC 8866 (SDP), RFC 8216 (HLS), RFC 8834/8835 (WebRTC media/transports), and RFC 9725 (WHIP).
- ONVIF Core, Streaming, Media/Media2 and Replay specifications, current specification history, Profiles T/G, and published conformance tests.
- AMWA NMOS IS-04/IS-05 for professional discovery/connection management; SMPTE ST 2110 for professional managed-IP essence.
- VSF TR-06 RIST family; SRT Alliance/Haivision published SRT protocol and reference implementation.
- GB/T 28181-2022 and legacy PSIA only when a deployment explicitly requires them.
