#pragma once

// P0-04 persistence — native increment 5a.
//
// An embedded local relational store (SQLite via the Windows-bundled
// winsqlite3) sitting behind the C0-03 adapter contract: declared-table schema,
// atomic writes with rollback, versioned forward migrations, and typed
// persistence errors (mirroring the reference `backend/file-db.js`). This is the
// STANDALONE local authority the owner selected for P0-04 (2026-07-26); the
// coordinated authority/sync model is deferred to P1-10.
//
// The class is Qt-free and depends only on SQLite, so it is unit-testable in
// isolation (`vms_dbtest`, like `vms_govtest`) and the backend stays swappable
// (system libsqlite3 / the amalgamation on other platforms) behind this
// interface. Scope + rationale: ../persistence-P0-04-proposal.md.

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "persist/Error.h"   // shared Status / Error (also used by SecretStore)

struct sqlite3;        // opaque; keeps winsqlite3.h out of this header
struct sqlite3_stmt;

namespace vms::persist {

// A cell value. Blobs are out of scope for the entities this pass persists.
using Value = std::variant<std::nullptr_t, std::int64_t, double, std::string>;
using Row = std::vector<Value>;

// One forward migration: applied once, in ascending version order, tracked in
// schema_migrations. Forward-only (no down-migrations in the standalone pass).
struct Migration {
    int version = 0;
    std::string name;
    std::string sql;   // may contain multiple statements
};

struct Result {
    std::vector<std::string> columns;
    std::vector<Row> rows;
};

class Store {
public:
    Store();
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    // Open (or create) the database at `path`; use ":memory:" for a transient
    // store. Enables foreign keys and WAL. Returns a typed error on failure.
    Error open(const std::string& path);
    void close();
    bool isOpen() const { return db_ != nullptr; }

    // Apply every migration whose version is not yet recorded, each in its own
    // transaction, in ascending order. Idempotent: re-running applies nothing.
    Error migrate(const std::vector<Migration>& migrations);
    int schemaVersion() const;   // highest applied version, 0 if none

    // Run a statement with no result set (DDL/DML). Params bind positionally (?).
    Error exec(const std::string& sql, const std::vector<Value>& params = {});

    // Run a query and collect the rows.
    Error query(const std::string& sql, const std::vector<Value>& params,
                Result& out);

    Error begin();
    Error commit();
    Error rollback();

    // Online backup of the open database to `destPath` (atomic snapshot copy).
    Error backupTo(const std::string& destPath);

    // RAII transaction: begins on begin(); rolls back on scope exit unless
    // commit() succeeded. This is how callers get all-or-nothing atomic writes.
    class Tx {
    public:
        explicit Tx(Store& store) : store_(store) {}
        ~Tx();
        Tx(const Tx&) = delete;
        Tx& operator=(const Tx&) = delete;
        Error begin();
        Error commit();
        bool active() const { return active_; }

    private:
        Store& store_;
        bool active_ = false;
    };

private:
    Error prepare(const std::string& sql, const std::vector<Value>& params,
                  sqlite3_stmt** stmt);
    Error fail(int code, const std::string& context) const;

    sqlite3* db_ = nullptr;
};

} // namespace vms::persist
