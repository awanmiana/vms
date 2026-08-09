#pragma once

// P1-06 — durable audit history (native increment 28). Scope:
// ../audit-log-P1-06-proposal.md.
//
// The append-only store behind the A0 command envelope's audit sink: one row
// per attempt (refusals included), each row hash-chained to its predecessor —
// row_hash = H(prev_hash ‖ canonical fields) — so editing or deleting ANY row
// breaks verification from that point on (tamper evidence). Immutability is
// enforced by contract at this layer: no update or delete API exists here.
// The hash function is injected (the app supplies SHA-256 via Qt; tests can
// supply their own), keeping this repo pure and dependency-free like the other
// vms_persist repos. Trusted external timestamps, login/config auditing, and
// retention policy are later P1-06 slices.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "persist/Store.h"

namespace vms::persist {

struct AuditEntry {
    std::int64_t id = 0;      // assigned on append
    std::string timeUtc;      // 'YYYY-MM-DD HH:MM:SS' (caller's clock)
    std::string source;       // 'palette' | 'api' | 'ui'
    std::string command;
    std::string args;         // canonical "name=value" rendering
    std::string outcome;      // OutcomeName(): 'ok' | 'needs-confirm' | ...
    std::string message;
    std::string prevHash;     // chain link (empty-string genesis)
    std::string rowHash;
    // Canonical v2 adds structured context while preserving v1 verification
    // for rows written before schema v15.
    int canonicalVersion = 2;
    std::string category = "command"; // auth|credential|access|configuration|command
    std::string actorId;               // stable identity; empty only pre-identity
    std::string subjectId;             // affected stable entity/resource
    std::string correlationId;         // request/job/trace correlation
    std::string beforeState;           // canonical redacted JSON/text snapshot
    std::string afterState;            // canonical redacted JSON/text snapshot
};

// An RFC 3161-capable timestamp service signs the request's SHA-256 imprint
// outside this persistence layer. The DER token is stored as base64 because
// Store intentionally has no blob type. `verified*` describes the verifier
// that cryptographically checked the token before it was admitted.
struct AuditAnchor {
    std::int64_t id = 0;
    std::int64_t firstAuditId = 0;
    std::int64_t lastAuditId = 0;
    std::string headHash;
    std::string hashAlgorithm = "sha-256";
    std::string requestedUtc;
    std::string tsaUri;
    std::string tsaPolicyOid;
    std::string tokenBase64;
    std::string verifiedUtc;
    std::string verifier;
};

using AnchorVerifyFn = std::function<bool(const AuditAnchor&)>;

// Hex digest of the input bytes. Injected so the repo carries no crypto dep.
using HashFn = std::function<std::string(const std::string&)>;

// The canonical byte string a row's hash covers (prev_hash ‖ fields, with an
// unambiguous separator). Exposed so verifiers and tests share one definition.
std::string AuditCanonical(const AuditEntry& e);

class AuditRepo {
public:
    explicit AuditRepo(Store& store) : store_(store) {}

    // Append one entry: links it to the current chain head, computes its row
    // hash, and inserts. Fills e.id/e.prevHash/e.rowHash. Append-only — this
    // class deliberately has no update or delete.
    Error append(AuditEntry& e, const HashFn& hash);

    // The most recent `limit` entries, newest first (0 = all).
    Error list(int limit, std::vector<AuditEntry>& out);

    Error count(std::int64_t& out);

    // Walk the whole chain oldest-first, recomputing every link. On success
    // `brokenAtId` is 0; on a break it is the first row whose stored hashes no
    // longer match (a tampered, re-ordered, or deleted-predecessor row).
    Error verifyChain(const HashFn& hash, std::int64_t& brokenAtId);

    // Snapshot the current chain head for an external RFC 3161 request.
    Error makeAnchorRequest(const std::string& requestedUtc, AuditAnchor& out);

    // Persist a timestamp receipt only after an injected cryptographic
    // verifier accepts it and only while it still matches the live chain head.
    Error recordVerifiedAnchor(AuditAnchor& anchor,
                               const AnchorVerifyFn& verifier);

    Error listAnchors(std::vector<AuditAnchor>& out);

private:
    Store& store_;
};

} // namespace vms::persist
