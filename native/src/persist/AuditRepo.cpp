#include "persist/AuditRepo.h"

namespace vms::persist {

std::string AuditCanonical(const AuditEntry& e) {
    // Length-prefixed fields make the encoding unambiguous (no separator can
    // be forged by field content).
    auto part = [](const std::string& s) {
        return std::to_string(s.size()) + ':' + s;
    };
    const std::string v1 = part(e.prevHash) + part(e.timeUtc) + part(e.source) +
                           part(e.command) + part(e.args) + part(e.outcome) +
                           part(e.message);
    if (e.canonicalVersion == 1) return v1;
    return part("2") + v1 + part(e.category) + part(e.actorId) +
           part(e.subjectId) + part(e.correlationId) + part(e.beforeState) +
           part(e.afterState);
}

Error AuditRepo::append(AuditEntry& e, const HashFn& hash) {
    if (!hash) return Error{Status::Misuse, "audit append needs a hash function"};
    if (e.canonicalVersion != 1 && e.canonicalVersion != 2)
        return Error{Status::Misuse, "unsupported audit canonical version"};

    // One transaction covers head-read + insert, so two appends cannot chain
    // to the same predecessor.
    Store::Tx tx(store_);
    if (Error err = tx.begin(); !err) return err;

    Result r;
    if (Error err = store_.query(
            "SELECT row_hash FROM audit_log ORDER BY id DESC LIMIT 1;", {}, r);
        !err)
        return err;
    e.prevHash = r.rows.empty()
                     ? std::string()   // genesis
                     : std::get<std::string>(r.rows[0][0]);
    e.rowHash = hash(AuditCanonical(e));

    if (Error err = store_.exec(
            "INSERT INTO audit_log(time_utc, source, command, args, outcome, "
            "message, prev_hash, row_hash, canonical_version, category, "
            "actor_id, subject_id, correlation_id, before_state, after_state) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
            {e.timeUtc, e.source, e.command, e.args, e.outcome, e.message,
             e.prevHash, e.rowHash, std::int64_t{e.canonicalVersion}, e.category,
             e.actorId, e.subjectId, e.correlationId, e.beforeState,
             e.afterState});
        !err)
        return err;

    Result rid;
    if (Error err = store_.query("SELECT last_insert_rowid();", {}, rid); !err)
        return err;
    e.id = std::get<std::int64_t>(rid.rows[0][0]);
    return tx.commit();
}

namespace {

AuditEntry rowToEntry(const Row& row) {
    AuditEntry e;
    e.id = std::get<std::int64_t>(row[0]);
    e.timeUtc = std::get<std::string>(row[1]);
    e.source = std::get<std::string>(row[2]);
    e.command = std::get<std::string>(row[3]);
    e.args = std::get<std::string>(row[4]);
    e.outcome = std::get<std::string>(row[5]);
    e.message = std::get<std::string>(row[6]);
    e.prevHash = std::get<std::string>(row[7]);
    e.rowHash = std::get<std::string>(row[8]);
    e.canonicalVersion = static_cast<int>(std::get<std::int64_t>(row[9]));
    e.category = std::get<std::string>(row[10]);
    e.actorId = std::get<std::string>(row[11]);
    e.subjectId = std::get<std::string>(row[12]);
    e.correlationId = std::get<std::string>(row[13]);
    e.beforeState = std::get<std::string>(row[14]);
    e.afterState = std::get<std::string>(row[15]);
    return e;
}

const char* kColumns =
    "id, time_utc, source, command, args, outcome, message, prev_hash, "
    "row_hash, canonical_version, category, actor_id, subject_id, "
    "correlation_id, before_state, after_state";

AuditAnchor rowToAnchor(const Row& row) {
    AuditAnchor a;
    a.id = std::get<std::int64_t>(row[0]);
    a.firstAuditId = std::get<std::int64_t>(row[1]);
    a.lastAuditId = std::get<std::int64_t>(row[2]);
    a.headHash = std::get<std::string>(row[3]);
    a.hashAlgorithm = std::get<std::string>(row[4]);
    a.requestedUtc = std::get<std::string>(row[5]);
    a.tsaUri = std::get<std::string>(row[6]);
    a.tsaPolicyOid = std::get<std::string>(row[7]);
    a.tokenBase64 = std::get<std::string>(row[8]);
    a.verifiedUtc = std::get<std::string>(row[9]);
    a.verifier = std::get<std::string>(row[10]);
    return a;
}

const char* kAnchorColumns =
    "id, first_audit_id, last_audit_id, head_hash, hash_algorithm, "
    "requested_utc, tsa_uri, tsa_policy_oid, token_base64, verified_utc, "
    "verifier";

} // namespace

Error AuditRepo::list(int limit, std::vector<AuditEntry>& out) {
    out.clear();
    std::string sql = std::string("SELECT ") + kColumns +
                      " FROM audit_log ORDER BY id DESC";
    if (limit > 0) sql += " LIMIT " + std::to_string(limit);
    sql += ";";
    Result r;
    if (Error err = store_.query(sql, {}, r); !err) return err;
    out.reserve(r.rows.size());
    for (const Row& row : r.rows) out.push_back(rowToEntry(row));
    return Error::success();
}

Error AuditRepo::count(std::int64_t& out) {
    Result r;
    if (Error err = store_.query("SELECT COUNT(*) FROM audit_log;", {}, r); !err)
        return err;
    out = std::get<std::int64_t>(r.rows[0][0]);
    return Error::success();
}

Error AuditRepo::verifyChain(const HashFn& hash, std::int64_t& brokenAtId) {
    brokenAtId = 0;
    if (!hash) return Error{Status::Misuse, "chain verify needs a hash function"};

    Result r;
    if (Error err = store_.query(std::string("SELECT ") + kColumns +
                                     " FROM audit_log ORDER BY id ASC;",
                                 {}, r);
        !err)
        return err;

    std::string expectedPrev;   // genesis
    for (const Row& row : r.rows) {
        const AuditEntry e = rowToEntry(row);
        if (e.prevHash != expectedPrev || e.rowHash != hash(AuditCanonical(e))) {
            brokenAtId = e.id;
            return Error::success();   // verification RAN fine; the chain broke
        }
        expectedPrev = e.rowHash;
    }
    return Error::success();
}

Error AuditRepo::makeAnchorRequest(const std::string& requestedUtc,
                                   AuditAnchor& out) {
    out = {};
    if (requestedUtc.empty())
        return Error{Status::Misuse, "anchor request needs a UTC timestamp"};
    Result r;
    if (Error err = store_.query(
            "SELECT id, row_hash FROM audit_log ORDER BY id ASC LIMIT 1;",
            {}, r);
        !err)
        return err;
    if (r.rows.empty())
        return Error{Status::NotFound, "cannot anchor an empty audit chain"};
    out.firstAuditId = std::get<std::int64_t>(r.rows[0][0]);
    if (Error err = store_.query(
            "SELECT id, row_hash FROM audit_log ORDER BY id DESC LIMIT 1;",
            {}, r);
        !err)
        return err;
    out.lastAuditId = std::get<std::int64_t>(r.rows[0][0]);
    out.headHash = std::get<std::string>(r.rows[0][1]);
    out.hashAlgorithm = "sha-256";
    out.requestedUtc = requestedUtc;
    return Error::success();
}

Error AuditRepo::recordVerifiedAnchor(AuditAnchor& anchor,
                                      const AnchorVerifyFn& verifier) {
    if (!verifier)
        return Error{Status::Misuse, "anchor receipt needs a verifier"};
    if (anchor.tsaUri.empty() || anchor.tokenBase64.empty() ||
        anchor.verifiedUtc.empty() || anchor.verifier.empty())
        return Error{Status::Misuse, "anchor receipt metadata is incomplete"};
    if (anchor.hashAlgorithm != "sha-256")
        return Error{Status::Misuse, "only SHA-256 audit anchors are accepted"};
    if (!verifier(anchor))
        return Error{Status::Crypto, "timestamp token verification failed"};

    Store::Tx tx(store_);
    if (Error err = tx.begin(); !err) return err;
    AuditAnchor current;
    if (Error err = makeAnchorRequest(anchor.requestedUtc, current); !err)
        return err;
    if (current.firstAuditId != anchor.firstAuditId ||
        current.lastAuditId != anchor.lastAuditId ||
        current.headHash != anchor.headHash)
        return Error{Status::Constraint,
                     "audit chain changed after the timestamp request"};

    if (Error err = store_.exec(
            "INSERT INTO audit_chain_anchor(first_audit_id, last_audit_id, "
            "head_hash, hash_algorithm, requested_utc, tsa_uri, "
            "tsa_policy_oid, token_base64, verified_utc, verifier) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
            {anchor.firstAuditId, anchor.lastAuditId, anchor.headHash,
             anchor.hashAlgorithm, anchor.requestedUtc, anchor.tsaUri,
             anchor.tsaPolicyOid, anchor.tokenBase64, anchor.verifiedUtc,
             anchor.verifier});
        !err)
        return err;
    Result rid;
    if (Error err = store_.query("SELECT last_insert_rowid();", {}, rid); !err)
        return err;
    anchor.id = std::get<std::int64_t>(rid.rows[0][0]);
    return tx.commit();
}

Error AuditRepo::listAnchors(std::vector<AuditAnchor>& out) {
    out.clear();
    Result r;
    if (Error err = store_.query(std::string("SELECT ") + kAnchorColumns +
                                     " FROM audit_chain_anchor ORDER BY id DESC;",
                                 {}, r);
        !err)
        return err;
    out.reserve(r.rows.size());
    for (const Row& row : r.rows) out.push_back(rowToAnchor(row));
    return Error::success();
}

} // namespace vms::persist
