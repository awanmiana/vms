// P0-04 persistence self-check (native increment 5a). Exercises the store's
// acceptance criteria (proposal §8) with no Qt, no display, no camera:
// durability across reopen, idempotent forward migrations, atomic rollback, and
// a backup/restore round-trip. Registered with CTest as `persist_selfcheck`.

#include "persist/Store.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace vms::persist;

namespace {

int failures = 0;

void check(bool cond, const std::string& what) {
    if (cond) {
        std::cout << "  ok  " << what << "\n";
    } else {
        std::cout << "  FAIL " << what << "\n";
        ++failures;
    }
}

// A minimal slice of the reference schema: enough to prove the store works.
std::vector<Migration> migrations() {
    return {
        {1, "core_devices_cameras",
         "CREATE TABLE devices ("
         "  id TEXT PRIMARY KEY, name TEXT NOT NULL,"
         "  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
         "  updated_at TEXT NOT NULL DEFAULT (datetime('now')));"
         "CREATE TABLE cameras ("
         "  id TEXT PRIMARY KEY, device_id TEXT NOT NULL,"
         "  name TEXT NOT NULL,"
         "  FOREIGN KEY(device_id) REFERENCES devices(id));"},
        {2, "camera_desired_tier",
         "ALTER TABLE cameras ADD COLUMN desired_tier TEXT NOT NULL "
         "DEFAULT 'main';"},
    };
}

std::string tmpPath(const std::string& name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

std::int64_t countRows(Store& s, const std::string& sql) {
    Result r;
    if (!s.query(sql, {}, r) || r.rows.empty()) return -1;
    return std::get<std::int64_t>(r.rows[0][0]);
}

} // namespace

int main() {
    std::cout << "vms_dbtest — P0-04 persistence self-check\n";

    const std::string dbFile = tmpPath("vms_dbtest.sqlite");
    const std::string bakFile = tmpPath("vms_dbtest_backup.sqlite");
    std::error_code ec;
    for (const std::string& f : {dbFile, dbFile + "-wal", dbFile + "-shm",
                                 bakFile})
        std::filesystem::remove(f, ec);

    // 1) Open + migrate.
    {
        Store s;
        check(static_cast<bool>(s.open(dbFile)), "open database file");
        check(static_cast<bool>(s.migrate(migrations())), "apply migrations");
        check(s.schemaVersion() == 2, "schema at version 2");

        // 2) Insert a device + camera (declared tables, positional binds).
        check(static_cast<bool>(s.exec(
                  "INSERT INTO devices(id, name) VALUES(?, ?);",
                  {std::string("dev-1"), std::string("Front NVR")})),
              "insert device");
        check(static_cast<bool>(s.exec(
                  "INSERT INTO cameras(id, device_id, name, desired_tier) "
                  "VALUES(?, ?, ?, ?);",
                  {std::string("cam-1"), std::string("dev-1"),
                   std::string("Lobby"), std::string("sub")})),
              "insert camera");

        // Constraint: the FK must reject an orphan camera.
        Error orphan = s.exec(
            "INSERT INTO cameras(id, device_id, name) VALUES(?, ?, ?);",
            {std::string("cam-x"), std::string("missing"),
             std::string("Orphan")});
        check(orphan.status == Status::Constraint,
              "foreign-key constraint rejects an orphan camera");
    }

    // 3) Durability: reopen and read the data back.
    {
        Store s;
        check(static_cast<bool>(s.open(dbFile)), "reopen database file");
        check(s.schemaVersion() == 2, "schema still at version 2 after reopen");
        check(countRows(s, "SELECT COUNT(*) FROM devices;") == 1,
              "device persisted across reopen");
        Result r;
        s.query("SELECT name, desired_tier FROM cameras WHERE id=?;",
                {std::string("cam-1")}, r);
        check(r.rows.size() == 1 &&
                  std::get<std::string>(r.rows[0][0]) == "Lobby" &&
                  std::get<std::string>(r.rows[0][1]) == "sub",
              "camera row persisted with its values");

        // 4) Migration idempotency: re-running applies nothing, stays valid.
        check(static_cast<bool>(s.migrate(migrations())),
              "re-running migrations is a no-op");
        check(s.schemaVersion() == 2, "schema unchanged after re-migrate");
        check(countRows(s, "SELECT COUNT(*) FROM schema_migrations;") == 2,
              "exactly two migrations recorded (not duplicated)");

        // 5) Atomicity: a rolled-back transaction leaves no trace.
        {
            Store::Tx tx(s);
            check(static_cast<bool>(tx.begin()), "begin transaction");
            check(static_cast<bool>(s.exec(
                      "INSERT INTO devices(id, name) VALUES(?, ?);",
                      {std::string("dev-tmp"), std::string("Temp")})),
                  "insert inside transaction");
            // tx goes out of scope WITHOUT commit -> rollback.
        }
        check(countRows(s, "SELECT COUNT(*) FROM devices;") == 1,
              "rolled-back insert left no row (atomic)");

        // 6) Backup round-trip.
        check(static_cast<bool>(s.backupTo(bakFile)), "backup to file");
    }
    {
        Store b;
        check(static_cast<bool>(b.open(bakFile)), "open the backup");
        check(countRows(b, "SELECT COUNT(*) FROM cameras;") == 1,
              "backup contains the camera row");
    }

    for (const std::string& f : {dbFile, dbFile + "-wal", dbFile + "-shm",
                                 bakFile})
        std::filesystem::remove(f, ec);

    if (failures == 0) {
        std::cout << "PASS: persistence store durable, migrated, atomic, and "
                     "backed up\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}
