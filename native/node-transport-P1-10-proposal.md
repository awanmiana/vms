# Node transport + AI-agent connector — scope proposal

> Status: **Discussion draft — NOT approved.** The controlling authority is
> `../Development_plan.md`. Per the Approval Contract (§2–4), no product code for
> this gate starts until its scope is discussed, explicitly approved, and recorded
> in the Approval Log. This document is the *discuss* artifact only. It mirrors the
> format of the other `native/*-proposal.md` files.
>
> Context (2026-07-29): the owner asked, from the first conversation, for the
> software's own **multi-deployment / node connectivity** to fail over to another
> IP/port (local/remote, over the network or internet) when a link degrades —
> possibly involving **AI agents / assistants** — and explicitly **NOT** for
> NVR/DVR/camera devices. A standalone failover *decision core* (increment 9) was
> built and then removed at the owner's request, because a standalone runtime has
> **no peers** to fail over between. This proposal is where that capability
> belongs: the **coordinated deployment**, where real peers exist. It is drafted
> for later implementation.

## 0. What this gate is, and is not

- **Is:** the network layer that connects *instances of this software* to each
  other — a client to an optional self-hosted coordinator, node to node, and this
  software to an **external AI agent** that drives it through the command/API
  envelope (A0). Includes endpoint **health + failover** (local → LAN → internet)
  and **active bandwidth probing** between nodes, because both need real peers.
- **Is not:** camera / NVR / DVR connectivity. That is the `ConnectionBroker`
  (increment 6) and stays untouched. This gate never opens a device stream.

## 1. Roadmap alignment (this is cross-cutting — scope one narrow first pass)

The coordinated deployment the P0-01 contract describes (P0-01C/F/I) is realized
by several roadmap items; this proposal is their connectivity substrate:

| Concern | Owning item(s) |
| :-- | :-- |
| Optional self-hosted coordination / shared metadata service | **P0-01F** (direction) + **P1-10** (config/sync/backup) |
| Programmatic API + queues + workers + site-agent protocols | **P1-05** + **P7-03** |
| External **AI-agent** control of every action (command envelope, capability-scoped, audited) | **A0 / P1-12 / P1-13** |
| Outage / failover / WAN behavior | **P0-03** + **P12-04** |

Because it touches unapproved gates, the honest move is a **narrow first pass**
behind clean interfaces, not the whole coordinated platform at once.

## 2. Intended users & benefit

- **Multi-site / control-room operators** — a client keeps working when its
  primary coordinator endpoint degrades by failing over to another (local → LAN →
  internet), instead of stalling.
- **Integrators / AI agents** — an external agent (or assistant) drives the
  software through a stable, authenticated, capability-scoped, audited API — the
  A0 mandate made real over a transport.
- **Benefit:** the "hybrid" topology the product promises (P0-01C) — self-contained
  standalone *plus* optional coordinated/scale-out — becomes real, with honest
  resilience (P0-03) instead of a single point of failure.

## 3. The decision space (owner's call, later)

| Option | Shape | Fits | Cost |
| :-- | :-- | :-- | :-- |
| **A. In-process client + one self-hosted coordinator** over a documented transport (e.g. HTTP/WebSocket or gRPC), with endpoint failover + an AI-agent API surface on the same transport | the standalone→coordinated step the product promises; smallest real coordinated deployment | must pick + secure a transport, define the wire contract, auth, and the sync model |
| **B. Full scale-out** (workers, site agents, load balancing, sharded metadata) | very large inventories (P0-09) | large; premature before A is proven |
| **C. Agent connector only** (no node-to-node), exposing the A0 command API to an external agent over a local transport | realizes A0 soonest, no coordinator | doesn't deliver multi-node failover (the owner's main ask) |

**Recommendation (for later discussion):** **A**, scoped to *one* client ↔ *one*
coordinator link with endpoint failover + the AI-agent API on the same transport,
behind interfaces so B/C extend it. Decide the exact transport, wire format, and
auth as the first sub-decision.

## 4. Included scope — narrow first pass (if approved later)

- **A node/endpoint model + failover policy** (re-establishing what increment 9
  prototyped, now with a real transport): preference-ordered endpoints (local →
  LAN → internet), P0-03 backoff/circuit-breaker health, `select()` → most-preferred
  usable endpoint, honest typed `Unavailable` when all are down.
- **A transport client** that connects to a coordinator endpoint, detects loss
  (heartbeat/timeout), and **fails over** to the next endpoint — never to a camera.
- **Active bandwidth probing between nodes** (see §5): measure the real usable
  throughput of a node link and feed it to the optimizer's bandwidth budget, so a
  congested WAN degrades honestly instead of relying on an operator estimate.
- **The AI-agent connector**: expose the A0 validated-command envelope
  (capability-checked, dangerous-action-confirmed, result-mapped, audited) over the
  transport so an external agent/assistant can drive the software — the same
  commands the UI/voice/text use (P1-12/P1-13).
- **Honest failure (P0-03)** and **security**: authenticated, encrypted transport;
  no plaintext secrets on the wire; every agent action audited (P1-06).
- All behind interfaces + a self-check harness (character of the existing
  `vms_*test` suites), with a fake transport so the failover/agent logic is
  unit-testable without a live peer.

## 5. Active bandwidth probing (what the owner asked "what is that?")

Two different things, often confused:

- **Passive measurement (demand):** watch how many bytes/sec the streams already
  flowing actually use. Doable locally; it refines the optimizer's per-stream
  *cost* estimates (today's 4000/1000/256 kbps defaults) with measured numbers.
- **Active probing (capacity):** send a short, bounded burst of test traffic to a
  **peer** (a coordinator/node/echo endpoint) and measure the achieved throughput
  and stability — that is the real *link budget* the optimizer's bandwidth budget
  needs, rather than an operator's guess.

Active probing **requires a peer to probe against** — exactly what a standalone
runtime lacks (the same reason node failover was removed from standalone). So it
belongs **here**, alongside the node transport, where real peers exist. When
built, its result flows straight into the increment-8 optimizer
(`CapacityProfile.bandwidthBudgetKbps`), which already enforces a bandwidth budget.
(A passive-measurement-only refinement of stream *costs* could be done earlier as a
small optimizer follow-up if the owner wants it before this gate.)

## 6. Explicitly deferred

- **Scale-out** (workers, site agents, load balancing, sharded/replicated
  metadata) — P7-03 / P0-09; Option B.
- **The coordinated authority / synchronization / conflict reconciliation model**
  — P1-10 proper (this gate carries *config/state* over the transport; the
  authoritative sync model is its own decision).
- **The concrete command catalog + capability/RBAC model** for the agent API —
  P1-02/P1-03/P1-12/P1-13 (this gate exposes the envelope; the catalog is theirs).
- **Durable audit sink** — P1-06.
- **HA / multi-coordinator quorum** — P12-04.

## 7. Measurable acceptance criteria (first pass, when built)

1. **Failover:** with the primary coordinator endpoint made unreachable, the client
   fails over to the next preferred endpoint within a bounded time and keeps
   working; when all are down it reports an honest, typed unavailable state (no
   false "connected").
2. **Preference + recovery:** local is preferred over LAN over internet; a
   recovered endpoint is used again (on success and after the breaker cooldown).
3. **Bandwidth probe:** an active probe against a peer yields a throughput figure
   that feeds the optimizer's bandwidth budget; a throttled link measurably lowers
   the admitted tiers (verified against the increment-8 optimizer).
4. **Agent control:** an external agent can invoke a validated command over the
   transport; capability checks, dangerous-action confirmation, deterministic
   result mapping, and an audit record all apply (A0 / P1-13).
5. **Security:** transport is authenticated + encrypted; no plaintext secret on the
   wire; unauthenticated/over-scope agent calls are rejected and audited.
6. **Boundary:** no path in this gate opens a camera/NVR/DVR connection (that
   remains the ConnectionBroker); a test asserts device connectivity is untouched.

## 8. Alternatives considered

- **Keep node failover in the standalone runtime** — rejected (and the built core
  removed): nothing to fail over between; it is coordinated-deployment behavior.
- **Build scale-out first (Option B)** — rejected: premature before one
  client↔coordinator link is proven.
- **Roll a bespoke binary protocol** — discouraged: prefer a documented, securable
  transport with existing tooling; decide in the first sub-decision.

## 9. Proposed build sequence (when approved)

- **9a — interfaces + fake transport:** node/endpoint model, failover policy
  (P0-03 breaker), the agent-command envelope surface, and a fake transport —
  the pure, unit-testable core (a `vms_nodetest`-style self-check).
- **9b — real transport client:** connect to a coordinator endpoint, heartbeat,
  failover; authenticated + encrypted; honest typed failure.
- **9c — active bandwidth probe:** measure a node link and feed the optimizer.
- **9d — AI-agent connector:** the A0 command envelope over the transport,
  capability-scoped + audited, verified by P1-13 coverage.

## 10. Recorded Approval Log scope (draft — for the owner to approve/edit later)

> Node transport + AI-agent connector | <date> | <decision> | Coordinated-
> deployment connectivity for this software's own instances (NOT cameras): a
> node/endpoint model + failover policy (local→LAN→internet, P0-03 breaker, honest
> Unavailable); a real authenticated+encrypted transport client to a self-hosted
> coordinator with heartbeat + failover; active bandwidth probing between nodes
> feeding the increment-8 optimizer's bandwidth budget; and an AI-agent connector
> exposing the A0 validated-command envelope (capability-scoped, confirmed,
> audited) over the transport. Behind interfaces with a fake transport so the
> failover/agent logic is unit-testable without a live peer. Realizes the P0-01C/F
> hybrid/coordinated direction and the A0 agent-control mandate. Deferred: scale-out
> (P7-03/P0-09), the authoritative sync/conflict model (P1-10 proper), the command
> catalog + RBAC (P1-02/03/12/13), durable audit (P1-06), and HA quorum (P12-04). |
> Acceptance criteria §7 | Camera/NVR/DVR connectivity is out of scope and
> untouched (ConnectionBroker). |
