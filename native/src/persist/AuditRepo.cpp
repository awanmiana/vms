#include "persist/AuditRepo.h"

namespace vms::persist {

std::string AuditCanonical(const AuditEntry& e) {
    // Length-prefixed fields make the encoding unambiguous (no separator can
    // be forged by field content).
    auto part = [](const std::string& s) {
        return std::to_string(s.size()) + ':' + s;
    };
    return part(e.prevHash) + part(e.timeUtc) + part(e.source) +
           part(e.command) + part(e.args) + part(e.outcome) + part(e.message);
}

Error AuditRepo::append(AuditEntry& e, const HashFn& hash) {
    if (!hash) return Error{Status::Misuse, "audit append needs a hash function"};

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
            "message, prev_hash, row_hash) VALUES(?, ?, ?, ?, ?, ?, ?, ?);",
            {e.timeUtc, e.source, e.command, e.args, e.outcome, e.message,
             e.prevHash, e.rowHash});
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
    return e;
}

const char* kColumns =
    "id, time_utc, source, command, args, outcome, message, prev_hash, "
    "row_hash";

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

} // namespace vms::persist
