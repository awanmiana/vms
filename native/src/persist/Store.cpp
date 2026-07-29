#include "persist/Store.h"

#include <winsqlite/winsqlite3.h>

#include <algorithm>

namespace vms::persist {

namespace {

Status statusForSqlite(int code) {
    switch (code) {
        case SQLITE_OK:
        case SQLITE_DONE:
        case SQLITE_ROW:        return Status::Ok;
        case SQLITE_CONSTRAINT: return Status::Constraint;
        case SQLITE_BUSY:
        case SQLITE_LOCKED:     return Status::Busy;
        case SQLITE_IOERR:
        case SQLITE_CANTOPEN:
        case SQLITE_READONLY:
        case SQLITE_CORRUPT:    return Status::Io;
        case SQLITE_MISUSE:     return Status::Misuse;
        default:                return Status::Error;
    }
}

} // namespace

Store::Store() = default;

Store::~Store() { close(); }

Error Store::fail(int code, const std::string& context) const {
    Error e;
    e.status = statusForSqlite(code);
    e.sqliteCode = code;
    const char* msg = db_ ? sqlite3_errmsg(db_) : sqlite3_errstr(code);
    e.message = context + ": " + (msg ? msg : "unknown error");
    return e;
}

Error Store::open(const std::string& path) {
    if (db_) return {Status::Misuse, "store already open", 0};
    const int rc = sqlite3_open_v2(
        path.c_str(), &db_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        // db_ may be non-null even on failure; capture the message then close.
        Error e = fail(rc, "open " + path);
        sqlite3_close(db_);
        db_ = nullptr;
        return e;
    }
    sqlite3_busy_timeout(db_, 3000);
    // Foreign keys on; WAL for durability + concurrent readers. WAL is a no-op
    // for :memory:, which is fine.
    if (Error e = exec("PRAGMA foreign_keys=ON;"); !e) return e;
    exec("PRAGMA journal_mode=WAL;");
    return Error::success();
}

void Store::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

Error Store::prepare(const std::string& sql, const std::vector<Value>& params,
                     sqlite3_stmt** stmt) {
    if (!db_) return {Status::Misuse, "store not open", 0};
    const int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, stmt, nullptr);
    if (rc != SQLITE_OK) return fail(rc, "prepare");

    for (std::size_t i = 0; i < params.size(); ++i) {
        const int idx = static_cast<int>(i) + 1;
        const Value& v = params[i];
        int brc = SQLITE_OK;
        if (std::holds_alternative<std::nullptr_t>(v)) {
            brc = sqlite3_bind_null(*stmt, idx);
        } else if (std::holds_alternative<std::int64_t>(v)) {
            brc = sqlite3_bind_int64(*stmt, idx, std::get<std::int64_t>(v));
        } else if (std::holds_alternative<double>(v)) {
            brc = sqlite3_bind_double(*stmt, idx, std::get<double>(v));
        } else {
            const std::string& s = std::get<std::string>(v);
            brc = sqlite3_bind_text(*stmt, idx, s.c_str(),
                                    static_cast<int>(s.size()), SQLITE_TRANSIENT);
        }
        if (brc != SQLITE_OK) {
            Error e = fail(brc, "bind");
            sqlite3_finalize(*stmt);
            *stmt = nullptr;
            return e;
        }
    }
    return Error::success();
}

Error Store::exec(const std::string& sql, const std::vector<Value>& params) {
    // Fast path for multi-statement DDL with no params (migrations).
    if (params.empty()) {
        if (!db_) return {Status::Misuse, "store not open", 0};
        char* err = nullptr;
        const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            Error e{statusForSqlite(rc), std::string("exec: ") +
                                             (err ? err : "unknown"), rc};
            sqlite3_free(err);
            return e;
        }
        return Error::success();
    }

    sqlite3_stmt* stmt = nullptr;
    if (Error e = prepare(sql, params, &stmt); !e) return e;
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) return fail(rc, "exec");
    return Error::success();
}

Error Store::query(const std::string& sql, const std::vector<Value>& params,
                   Result& out) {
    out.columns.clear();
    out.rows.clear();

    sqlite3_stmt* stmt = nullptr;
    if (Error e = prepare(sql, params, &stmt); !e) return e;

    const int cols = sqlite3_column_count(stmt);
    out.columns.reserve(cols);
    for (int c = 0; c < cols; ++c) {
        const char* name = sqlite3_column_name(stmt, c);
        out.columns.emplace_back(name ? name : "");
    }

    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        Row row;
        row.reserve(cols);
        for (int c = 0; c < cols; ++c) {
            switch (sqlite3_column_type(stmt, c)) {
                case SQLITE_INTEGER:
                    row.emplace_back(
                        static_cast<std::int64_t>(sqlite3_column_int64(stmt, c)));
                    break;
                case SQLITE_FLOAT:
                    row.emplace_back(sqlite3_column_double(stmt, c));
                    break;
                case SQLITE_NULL:
                    row.emplace_back(nullptr);
                    break;
                default: {
                    const unsigned char* t = sqlite3_column_text(stmt, c);
                    const int n = sqlite3_column_bytes(stmt, c);
                    row.emplace_back(std::string(
                        reinterpret_cast<const char*>(t ? t : (const unsigned char*)""),
                        static_cast<std::size_t>(n)));
                    break;
                }
            }
        }
        out.rows.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return fail(rc, "query");
    return Error::success();
}

Error Store::begin()    { return exec("BEGIN IMMEDIATE;"); }
Error Store::commit()   { return exec("COMMIT;"); }
Error Store::rollback() { return exec("ROLLBACK;"); }

int Store::schemaVersion() const {
    if (!db_) return 0;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "SELECT COALESCE(MAX(version),0) FROM schema_migrations;", -1,
            &stmt, nullptr) != SQLITE_OK) {
        return 0;  // table absent yet => version 0
    }
    int v = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) v = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return v;
}

Error Store::migrate(const std::vector<Migration>& migrations) {
    if (!db_) return {Status::Misuse, "store not open", 0};

    if (Error e = exec(
            "CREATE TABLE IF NOT EXISTS schema_migrations ("
            "  version INTEGER PRIMARY KEY,"
            "  name TEXT NOT NULL,"
            "  applied_at TEXT NOT NULL DEFAULT (datetime('now')));");
        !e) {
        return {Status::Migration, "create schema_migrations: " + e.message,
                e.sqliteCode};
    }

    // Apply in ascending version order for determinism.
    std::vector<Migration> ordered = migrations;
    std::sort(ordered.begin(), ordered.end(),
              [](const Migration& a, const Migration& b) {
                  return a.version < b.version;
              });

    const int current = schemaVersion();
    for (const Migration& m : ordered) {
        if (m.version <= current) continue;  // already applied

        Tx tx(*this);
        if (Error e = tx.begin(); !e)
            return {Status::Migration, "begin migration " +
                                           std::to_string(m.version) + ": " +
                                           e.message, e.sqliteCode};
        if (Error e = exec(m.sql); !e)
            return {Status::Migration, "migration " + std::to_string(m.version) +
                                           " (" + m.name + "): " + e.message,
                    e.sqliteCode};
        if (Error e = exec(
                "INSERT INTO schema_migrations(version, name) VALUES(?, ?);",
                {static_cast<std::int64_t>(m.version), m.name});
            !e)
            return {Status::Migration, "record migration " +
                                           std::to_string(m.version) + ": " +
                                           e.message, e.sqliteCode};
        if (Error e = tx.commit(); !e)
            return {Status::Migration, "commit migration " +
                                           std::to_string(m.version) + ": " +
                                           e.message, e.sqliteCode};
    }
    return Error::success();
}

Error Store::backupTo(const std::string& destPath) {
    if (!db_) return {Status::Misuse, "store not open", 0};
    sqlite3* dest = nullptr;
    int rc = sqlite3_open_v2(destPath.c_str(), &dest,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        Error e = fail(rc, "open backup dest " + destPath);
        sqlite3_close(dest);
        return e;
    }
    sqlite3_backup* bk = sqlite3_backup_init(dest, "main", db_, "main");
    Error result = Error::success();
    if (bk) {
        sqlite3_backup_step(bk, -1);   // copy all pages at once
        sqlite3_backup_finish(bk);
        rc = sqlite3_errcode(dest);
        if (rc != SQLITE_OK) result = {statusForSqlite(rc), "backup step", rc};
    } else {
        result = {statusForSqlite(sqlite3_errcode(dest)), "backup_init",
                  sqlite3_errcode(dest)};
    }
    sqlite3_close(dest);
    return result;
}

// --- Tx --------------------------------------------------------------------

Store::Tx::~Tx() {
    if (active_) store_.rollback();
}

Error Store::Tx::begin() {
    if (active_) return {Status::Misuse, "transaction already active", 0};
    Error e = store_.begin();
    if (e) active_ = true;
    return e;
}

Error Store::Tx::commit() {
    if (!active_) return {Status::Misuse, "no active transaction", 0};
    Error e = store_.commit();
    active_ = false;   // committed or failed, the transaction is over
    return e;
}

} // namespace vms::persist
