// P0-04 persistence self-check (native increments 5a–5c). No Qt, no display, no
// camera. Exercises the canonical schema, durability across reopen, idempotent
// forward migrations, atomic rollback, the workspace layout round-trip (5c), and
// a backup/export round-trip. Registered with CTest as `persist_selfcheck`.

#include "persist/Schema.h"
#include "persist/Store.h"
#include "persist/WorkspaceRepo.h"
#include "persist/PremisesRepo.h"
#include "persist/DeviceRepo.h"
#include "persist/StreamDurationRepo.h"

#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace vms::persist;

namespace {

int failures = 0;

void check(bool cond, const std::string& what) {
    std::cout << (cond ? "  ok  " : "  FAIL ") << what << "\n";
    if (!cond) ++failures;
}

std::string tmpPath(const std::string& name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

std::int64_t count(Store& s, const std::string& sql) {
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

    // 1) Open + migrate the canonical schema.
    {
        Store s;
        check(static_cast<bool>(s.open(dbFile)), "open database file");
        check(static_cast<bool>(s.migrate(coreMigrations())),
              "apply canonical migrations");
        check(s.schemaVersion() == 15, "schema at version 15");

        // 2) Insert a device + credential ref + camera (declared tables).
        check(static_cast<bool>(s.exec(
                  "INSERT INTO devices(id, name, address, vendor) "
                  "VALUES(?, ?, ?, ?);",
                  {std::string("dev-1"), std::string("Front NVR"),
                   std::string("192.168.0.254"), std::string("Hikvision")})),
              "insert device");
        check(static_cast<bool>(s.exec(
                  "INSERT INTO device_credentials(device_id, credential_ref) "
                  "VALUES(?, ?);",
                  {std::string("dev-1"), std::string("cred://dev-1")})),
              "insert credential ref");
        check(static_cast<bool>(s.exec(
                  "INSERT INTO cameras(id, device_id, name, main_url, sub_url) "
                  "VALUES(?, ?, ?, ?, ?);",
                  {std::string("cam-1"), std::string("dev-1"),
                   std::string("Lobby"), std::string("rtsp://.../101"),
                   std::string("rtsp://.../102")})),
              "insert camera");

        Error orphan = s.exec(
            "INSERT INTO cameras(id, device_id, name) VALUES(?, ?, ?);",
            {std::string("cam-x"), std::string("missing"), std::string("Orphan")});
        check(orphan.status == Status::Constraint,
              "foreign-key constraint rejects an orphan camera");
    }

    // 3) Durability + idempotency + atomicity on reopen.
    {
        Store s;
        check(static_cast<bool>(s.open(dbFile)), "reopen database file");
        check(s.schemaVersion() == 15, "schema still at version 15 after reopen");
        check(count(s, "SELECT COUNT(*) FROM devices;") == 1,
              "device persisted across reopen");
        check(count(s, "SELECT COUNT(*) FROM cameras;") == 1,
              "camera persisted across reopen");

        check(static_cast<bool>(s.migrate(coreMigrations())),
              "re-running migrations is a no-op");
        check(count(s, "SELECT COUNT(*) FROM schema_migrations;") == 15,
              "exactly fifteen migrations recorded (not duplicated)");

        {
            Store::Tx tx(s);
            check(static_cast<bool>(tx.begin()), "begin transaction");
            check(static_cast<bool>(s.exec(
                      "INSERT INTO devices(id, name) VALUES(?, ?);",
                      {std::string("dev-tmp"), std::string("Temp")})),
                  "insert inside transaction");
            // no commit -> RAII rollback
        }
        check(count(s, "SELECT COUNT(*) FROM devices;") == 1,
              "rolled-back insert left no row (atomic)");

        // 4) Workspace layout round-trip (5c).
        WorkspaceRepo repo(s);
        InstanceState in;
        in.tileCount = 9;
        // tile0 Main/High at a dragged spatial position (v9); tile5 Thumb/Med
        // with the -1 "unset" default (falls back to the grid placement).
        in.tiles = {TilePref{0, 3, 3, 512.5, 380.0, 135.0, 92.0},
                    TilePref{5, 1, 2}};
        check(static_cast<bool>(repo.save("live", in)), "save workspace layout");

        PremisesRepo premises(s);
        check(static_cast<bool>(premises.ensureDefault("live")),
              "seed default site and floor");
        check(static_cast<bool>(premises.configureSite(
                  "live", "hq", "Headquarters", "Asia/Karachi")),
              "configure active site");
        check(static_cast<bool>(premises.configureFloor(
                  "live", "hq-1", "hq", "Ground floor",
                  "file:///plans/hq-ground.png", 2400.0, 1400.0)),
              "configure active floor and plan metadata");

        StreamDurationRepo streams(s);
        StreamDurationSummary emptyStream;
        check(static_cast<bool>(streams.summary("hq", emptyStream)) &&
                  emptyStream.milliseconds == 0 &&
                  emptyStream.checkpoints == 0,
              "connected stream-duration source reports honest zero");
        check(static_cast<bool>(streams.addObservation("hq", 1500, 2)) &&
                  static_cast<bool>(streams.addObservation("hq", 250, 1)),
              "playing branches accumulate monotonic camera-time");
        const Error negativeStream = streams.addObservation("hq", -1, 1);
        const Error overflowStream = streams.addObservation(
            "hq", std::numeric_limits<std::int64_t>::max(), 2);
        check(negativeStream.status == Status::Misuse &&
                  overflowStream.status == Status::Misuse,
              "negative and overflowing stream observations are refused");
        StreamDurationSummary streamTotal;
        check(static_cast<bool>(streams.summary("hq", streamTotal)) &&
                  streamTotal.milliseconds == 3250 &&
                  streamTotal.checkpoints == 2 &&
                  !streamTotal.updatedAtUtc.empty(),
              "stream duration preserves milliseconds and checkpoint evidence");
        check(static_cast<bool>(premises.addWeeklyWindow("hq", 1, 540, 720)) &&
                  static_cast<bool>(premises.addWeeklyWindow("hq", 1, 780, 1020)),
              "configure split Monday operating windows");
        const Error overlap = premises.addWeeklyWindow("hq", 1, 660, 840);
        check(overlap.status == Status::Misuse &&
                  count(s, "SELECT COUNT(*) FROM premises_hours_weekly;") == 2,
              "overlapping window is rejected atomically");
        check(static_cast<bool>(premises.clearWeeklyDay("hq", 2)),
              "configure Tuesday explicitly closed");
        check(static_cast<bool>(premises.setDateException(
                  "hq", "2026-12-25", true, 0, 0, "Public holiday")) &&
                  static_cast<bool>(premises.setDateException(
                      "hq", "2026-12-31", false, 600, 840,
                      "Year-end hours")),
              "configure closed and special-hours date exceptions");

        InMemorySecretStore observationSecrets;
        DeviceRepo devices(s, observationSecrets);
        check(static_cast<bool>(devices.setDeviceSite("dev-1", "hq")),
              "assign device to canonical premises site");
        const Error badSite = devices.setDeviceSite("dev-1", "missing-site");
        check(badSite.status == Status::NotFound,
              "unknown site assignment is refused");
        check(static_cast<bool>(devices.recordReachObservation(
                  "dev-1", true, "2026-08-03 08:00:00")) &&
                  static_cast<bool>(devices.recordReachObservation(
                      "dev-1", true, "2026-08-03 09:00:00")),
              "positive repeat advances last seen and preserves uptime start");
        check(static_cast<bool>(devices.recordReachObservation(
                  "dev-1", false, "2026-08-03 10:00:00")),
              "negative observation ends the current uptime run");
        const Error stale = devices.recordReachObservation(
            "dev-1", true, "2026-08-03 09:30:00");
        check(stale.status == Status::Misuse,
              "out-of-order reachability evidence is refused atomically");
        check(static_cast<bool>(devices.recordReachObservation(
                  "dev-1", true, "2026-08-03 11:00:00")),
              "positive after offline begins a new uptime run");
    }
    {
        Store s;
        check(static_cast<bool>(s.open(dbFile)), "reopen for layout restore");
        WorkspaceRepo repo(s);
        InstanceState out;
        bool found = false;
        check(static_cast<bool>(repo.load("live", out, found)) && found,
              "load saved workspace layout");
        check(out.tileCount == 9, "restored tile count");
        check(out.tiles.size() == 2 && out.tiles[0].id == 0 &&
                  out.tiles[0].desiredTier == 3 && out.tiles[0].priority == 3 &&
                  out.tiles[1].id == 5 && out.tiles[1].desiredTier == 1,
              "restored per-tile overrides");
        check(out.tiles.size() == 2 && out.tiles[0].posX == 512.5 &&
                  out.tiles[0].posY == 380.0 && out.tiles[1].posX == -1.0,
              "restored spatial positions (dragged + unset default)");
        check(out.tiles.size() == 2 && out.tiles[0].facingDeg == 135.0 &&
                  out.tiles[0].fovDeg == 92.0 && out.tiles[1].fovDeg == 70.0,
              "restored camera facing/FOV (custom + defaults)");

        PremisesRepo premises(s);
        PremisesState p;
        bool premisesFound = false;
        check(static_cast<bool>(premises.loadActive("live", p, premisesFound)) &&
                  premisesFound && p.siteId == "hq" && p.siteName == "Headquarters" &&
                  p.timezone == "Asia/Karachi" && p.floorId == "hq-1" &&
                  p.planUri == "file:///plans/hq-ground.png" &&
                  p.worldWidth == 2400.0 && p.worldHeight == 1400.0,
              "site/floor selection and plan metadata survive reopen");
        OperatingSchedule hours;
        check(static_cast<bool>(premises.loadOperatingSchedule("hq", hours)) &&
                  hours.configured && hours.weekly.size() == 2 &&
                  hours.exceptions.size() == 2,
              "operating schedule and exceptions survive reopen");
        check(hours.weekly[0].weekday == 1 &&
                  hours.weekly[0].startMinute == 540 &&
                  hours.weekly[1].endMinute == 1020 &&
                  hours.exceptions[0].closed &&
                  hours.exceptions[1].windows.size() == 1,
              "weekly and exception window values round-trip exactly");
        InMemorySecretStore restoredSecrets;
        DeviceRepo restoredDevices(s, restoredSecrets);
        std::vector<DeviceSummary> deviceRows;
        check(static_cast<bool>(restoredDevices.listDevices(deviceRows)) &&
                  deviceRows.size() == 1 && deviceRows[0].siteId == "hq",
              "device-site ownership survives reopen and bad assignment");
        check(deviceRows[0].reachObserved &&
                  deviceRows[0].persistedReachState == "online" &&
                  deviceRows[0].reachObservedAtUtc == "2026-08-03 11:00:00" &&
                  deviceRows[0].lastSeenUtc == "2026-08-03 11:00:00" &&
                  deviceRows[0].onlineSinceUtc == "2026-08-03 11:00:00",
              "reach state, last seen, and restarted uptime run survive reopen");
        StreamDurationRepo restoredStreams(s);
        StreamDurationSummary restoredStream;
        check(static_cast<bool>(restoredStreams.summary("hq", restoredStream)) &&
                  restoredStream.milliseconds == 3250 &&
                  restoredStream.checkpoints == 2,
              "stream duration survives reopen without filling downtime");

        // save again (atomic replace) — still one instance row, new tiles.
        InstanceState in2;
        in2.tileCount = 4;
        in2.tiles = {TilePref{1, 2, 2}};
        check(static_cast<bool>(repo.save("live", in2)),
              "re-save layout (atomic replace)");
        check(count(s, "SELECT COUNT(*) FROM workspace_instance;") == 1,
              "one instance row after re-save (no duplicate)");
        check(count(s, "SELECT COUNT(*) FROM workspace_tile "
                       "WHERE instance='live';") == 1,
              "old tiles replaced, not appended");

        // 5) A first-run instance loads as not-found (not an error).
        InstanceState pb;
        bool pbFound = true;
        check(static_cast<bool>(repo.load("playback", pb, pbFound)) && !pbFound,
              "unsaved instance loads as not-found");

        // 6) Backup / export round-trip.
        check(static_cast<bool>(s.backupTo(bakFile)), "backup/export to file");
    }
    {
        Store b;
        check(static_cast<bool>(b.open(bakFile)), "open the exported snapshot");
        check(count(b, "SELECT COUNT(*) FROM cameras;") == 1,
              "export contains the camera row");
        check(count(b, "SELECT tile_count FROM workspace_instance "
                       "WHERE name='live';") == 4,
              "export contains the latest layout");
    }

    for (const std::string& f : {dbFile, dbFile + "-wal", dbFile + "-shm",
                                 bakFile})
        std::filesystem::remove(f, ec);

    if (failures == 0) {
        std::cout << "PASS: schema, durability, atomicity, layout round-trip, "
                     "and export all verified\n";
        return 0;
    }
    std::cout << "FAILED: " << failures << " check(s)\n";
    return 1;
}
