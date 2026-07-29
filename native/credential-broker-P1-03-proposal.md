# Connection broker + credential store (DPAPI) — scope proposal

> Status: **Scope approved 2026-07-27 (Option A — DPAPI); increment 6a built &
> verified on the dev box.** The owner approved the embedded, standalone-first
> direction (in-process broker + Windows DPAPI secret store behind a swappable
> `SecretStore` interface). Increment 6a (the `SecretStore` interface + DPAPI
> impl + in-memory double + `vms_credtest`/CTest) is built and green; 6b/6c
> remain. The controlling authority remains `../Development_plan.md`. This
> gate realizes the **N0 connection-broker decision** (`ARCHITECTURE.md` §"Decision:
> centralized connection broker for device access") and the **secrets-storage
> clause of P1-03** (`[ ] Discussed | [ ] Approved`). It is the gate P0-04
> explicitly deferred to: the store today holds only a `credential_ref`
> (`src/persist/Schema.cpp:19`) — this gate stores the actual secret and puts a
> single broker identity in front of every device connection. Mirrors the format
> of `governor-P3-03-proposal.md` and `persistence-P0-04-proposal.md`. Builds as
> **native increment 6**.

## 1. The decision this gate actually makes

This gate makes two coupled decisions:

1. **Where device secrets live and how they are protected.** ARCHITECTURE.md
   already commits the direction — *"App stores only a `credential_ref`; secrets
   never in plaintext"*, Windows **DPAPI** now, libsecret/keyring on Linux later.
   This gate turns that committed direction into a real, swappable secret store and
   defines what "protected at rest" concretely means for the standalone runtime.
2. **What a "connection broker" is in the standalone runtime.** ARCHITECTURE.md
   commits *"Only the broker connects to devices … the single connection identity
   to each device,"* with *"same design, two deployments"* (standalone = in-process
   broker with DPAPI; coordinated = shared server with a vault, RBAC, audit — a
   later gate). This gate builds the **in-process standalone broker** behind an
   interface the coordinated server can later implement unchanged.

Both are framed conservatively below; the owner makes the call.

## 2. Intended users

- **Administrators** — provision a device once with its credentials; the secret is
  encrypted at rest and never shown again in plaintext.
- **Operators** — authenticate to the **VMS** (their own identity), never receive or
  handle device credentials, and never connect to NVRs/DVRs directly.
- **Both deployment shapes** — standalone (in-process broker + DPAPI) and, later,
  coordinated (shared broker + vault); identical operator/broker interface (N0).

## 3. Benefit

The root-cause fixes ARCHITECTURE.md already names: **leaver cleanup** (disable the
VMS account centrally; the person never held a device credential) and **device
lockout avoidance** (one connection identity + pooling that respects device
max-session limits + login backoff/circuit-breaker per the P0-03 resilience
policy). Today `vms_grid --camera` and any future onboarding take a full RTSP URL
*with the password inline* on the command line — there is no secret store and no
single connection identity. This gate closes that gap for the standalone runtime.

## 4. The decision space (this is the owner's call)

| Option | Shape | Fits | Cost |
| :- | :- | :- | :- |
| **A. In-process broker + OS secret store (DPAPI)** | secret encrypted per-user/per-machine by the OS; broker is a local library | standalone-first; matches N0 "user installs nothing" and the committed ARCHITECTURE record | DPAPI scope is per Windows user/machine — a copied DB file is useless off-box (a feature, but constrains portable export) |
| **B. App-managed master key (passphrase-derived)** | app derives a key (e.g. from an operator passphrase) and encrypts the secret column itself | portable across machines; cross-platform with one code path | must design KDF, key storage, unlock UX, and recovery — front-loads P1-03 password policy |
| **C. External vault / server-held secrets** | secrets never on the client | the coordinated deployment | requires a server; premature for standalone (this is P1-10 / the coordinated gate) |

**Recommendation (for discussion, not a decision):** adopt **A** for the standalone
runtime — the in-process broker plus **DPAPI** as the secret store — behind a
technology-neutral `SecretStore` interface, and **defer B/C**. This is exactly what
the N0 ARCHITECTURE record already commits, keeps "user installs nothing" intact,
and lets the coordinated server later implement the same broker + `SecretStore`
interface against a real vault without changing the client. A DPAPI-encrypted
secret is bound to the Windows user/machine, which is the correct default for a
local authority. If you would rather adopt B now (portable, passphrase-derived) that
enlarges this gate into P1-03 password-policy territory and should be decided first.

## 5. Included scope (first pass, if the recommendation is approved)

- A **`SecretStore` interface** (Qt-free, swappable, unit-testable in isolation like
  `Store`) with a **Windows DPAPI implementation** (`CryptProtectData` /
  `CryptUnprotectData`, `CRYPTPROTECT_LOCAL_MACHINE` decision documented) and an
  **in-memory test double** so the self-check runs without touching the real OS
  store. Secrets are `put(credential_ref, plaintext)` / `get(credential_ref)` /
  `remove(credential_ref)`; ciphertext at rest, plaintext only in memory for the
  lifetime of a connection attempt, zeroized after use.
- **Wiring to the existing store:** `device_credentials.credential_ref`
  (`Schema.cpp`) becomes the opaque handle the `SecretStore` resolves. No plaintext
  ever enters SQLite. A repository (`CredentialRepo`) mints/looks up the ref and
  brokers the store↔secret-store pairing atomically (both succeed or neither).
- A **standalone `ConnectionBroker`**: the single component that, given a
  `camera_id` + tier, resolves the device's `credential_ref` → secret → builds the
  connection URL **in memory** and hands GStreamer a ready session. Callers
  (`vms_grid`, `vms_workspace`) pass a `camera_id`, **never a URL-with-password**.
  The broker owns connection pooling + login backoff/circuit-breaker hooks (the
  P0-03 policy interface; the policy itself is already an approved contract).
- **Credential-free everywhere it already is:** the broker keeps the existing
  discipline — URLs/secrets are never logged; only a credential-free
  `stream selection:` / `connection:` line is printed (as `--govern --camera`
  already does).
- **Honest failure (P0-03C/P0-03E):** a missing/undecryptable secret, or a device
  that is locked out, fails with a **typed error** — never a silent retry loop,
  never a plaintext fallback, never "connected" when it isn't.
- A **`vms_credtest` CTest self-check** (the character of `vms_govtest` /
  `vms_dbtest`): round-trip put/get/remove against the in-memory double, ref↔store
  atomicity, no-plaintext-in-DB assertion, typed-error paths, and (guarded, dev-box
  only) a real DPAPI round-trip.

## 6. Explicitly deferred (out of this pass)

- **Coordinated broker** — shared server, vault, RBAC, audit, cross-node identity →
  the coordinated gate (P1-10 / its own gate). This pass builds only the in-process
  standalone broker behind the shared interface.
- **Operator authentication, password policy, MFA/step-up, lockout, recovery,
  service accounts** — the rest of **P1-03**. This pass stores *device* secrets; it
  does not build *operator* login. (If the owner picks Option B, some of this is
  pulled forward — a reason to prefer A.)
- **Immutable audit of credential use** — **P1-06**. The broker exposes the hooks
  (who resolved which ref, when, outcome) but the durable audit sink is P1-06.
- **Linux secret store** (libsecret/keyring) — a second `SecretStore` impl behind
  the same interface, when a Linux build is targeted.
- **ONVIF/vendor credential negotiation & discovery** — P2-05/P2-14. This pass wires
  operator-provided credentials only, exactly as 4b did for URLs.

## 7. Reference alignment

The behavioral spec is the reference backend's device-adapter/credential handling
and the P0-03 resilience policy modules (`backend/`), plus the N0 ARCHITECTURE
broker decision. The native `SecretStore` mirrors the **committed direction and the
adapter/credential semantics**, not any JSON format. C0-03 already put atomic
replace / typed errors behind a boundary; P0-04 provided the first production
persistence impl; this gate adds the secret store + broker behind the same style of
technology-neutral interface. Per mandate **A0**, broker/credential operations are
built behind a clean interface so they can later be expressed as validated commands
(the command envelope itself is P1-12/P1-13).

## 8. Measurable acceptance criteria

1. **No plaintext at rest:** device secrets never appear in the SQLite file or any
   log; a scan of the DB and captured output after provisioning finds only the
   ciphertext handle / `credential_ref` (asserted by the self-check).
2. **Round-trip:** a secret stored via the `SecretStore` decrypts to the original
   plaintext on the same user/machine; on DPAPI, a copied DB on a different
   user/machine fails to decrypt with a typed error (documents the binding).
3. **Single connection identity:** callers connect by `camera_id`; the broker is the
   only component that materializes a credentialed URL, and it does so in memory
   only (verified: no caller path takes a password).
4. **Atomicity:** provisioning a device writes the `credential_ref` row and the
   secret together — a failure of either leaves neither (RAII rollback + secret
   store remove), no orphaned ref and no orphaned secret.
5. **Honest failure:** a missing/undecryptable secret or a locked-out device fails
   with a typed error; nothing is silently retried into a lockout and nothing is
   reported connected when it is not (P0-03C/E).
6. **Backoff/pooling:** repeated failed connects to one device trigger the P0-03
   backoff/circuit-breaker via the broker (the device is not hammered), and the
   broker respects a configured max-session count per device.
7. **Boundary:** all secret access goes through the `SecretStore` interface and all
   device connection goes through the broker; no consumer reaches DPAPI or builds a
   credentialed URL directly.

## 9. Alternatives considered

- **Keep passing full URLs-with-passwords** (current spike behavior) — rejected as
  production behavior: the secret lives in shell history / process args / logs and
  there is no single connection identity, defeating both root-cause fixes.
- **Encrypt the whole SQLite file** (e.g. SQLCipher) — rejected for the first pass:
  it protects the file but not the "single broker identity" goal, adds a vendored
  crypto dependency against the "user installs nothing / winsqlite3" choice, and
  still needs a key-management answer (Option B's cost) without the OS doing it.
- **Decide the coordinated vault now** — rejected for a standalone-first pass: it
  front-loads server, RBAC, and audit before there is a coordinated deployment to
  serve (that is P1-10).

## 10. Proposed build sequence (native increment 6, each verified on hardware)

- **6a — `SecretStore` + DPAPI spike:** the interface, the DPAPI impl, the in-memory
  double, and `vms_credtest`/CTest (no-plaintext, round-trip, typed errors). Char of
  increments 1–3 / `vms_govtest` / `vms_dbtest`.
- **6b — credential wiring + atomicity:** `CredentialRepo` binding
  `device_credentials.credential_ref` ↔ `SecretStore` atomically; the DB-has-no-
  plaintext and ref/secret-atomicity acceptance tests.
- **6c — standalone `ConnectionBroker`:** connect-by-`camera_id`, in-memory URL
  materialization, pooling + P0-03 backoff hooks, honest typed failures; rewire
  `vms_grid`/`vms_workspace` to go through the broker instead of raw
  URL-with-password. Live-camera confirmation with the test camera's credentials
  (shares the hardware-blocked item already tracked for 4b).

## 11. Recorded Approval Log scope (draft — for the owner to approve/edit)

> Connection broker + credential store (DPAPI) | <date> | Approved (scope) | First
> broker/secret pass, standalone runtime: a Qt-free swappable `SecretStore`
> interface with a Windows **DPAPI** implementation and an in-memory test double
> (secrets encrypted at rest, plaintext in memory only for a connection attempt);
> `device_credentials.credential_ref` resolved through it with atomic ref↔secret
> writes; an in-process `ConnectionBroker` as the single device-connection identity
> (connect-by-`camera_id`, in-memory URL materialization, connection pooling + P0-03
> backoff/circuit-breaker hooks, honest typed failure per P0-03C/E); callers stop
> passing URLs-with-passwords; `vms_credtest`/CTest. Realizes the N0 broker decision
> and the secrets-storage clause of P1-03 for the standalone runtime. Deferred: the
> coordinated broker/vault/RBAC (P1-10), operator authentication + password policy +
> MFA (rest of P1-03), durable credential audit (P1-06), the Linux secret store, and
> ONVIF/vendor credential negotiation (P2-05/P2-14). Built behind a clean interface
> so it can later be a validated command (A0/P1-12/P1-13). | Acceptance criteria §8
> | Selects the standalone in-process broker + DPAPI only; the coordinated authority
> model remains P1-10. | Building as native increment 6.
