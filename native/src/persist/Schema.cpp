#include "persist/Schema.h"

namespace vms::persist {

std::vector<Migration> coreMigrations() {
    return {
        // v1 — core inventory: devices, their credential reference (secret is the
        // DPAPI gate, not here), cameras, and camera groups.
        {1, "core_inventory",
         "CREATE TABLE devices ("
         "  id TEXT PRIMARY KEY,"
         "  name TEXT NOT NULL,"
         "  address TEXT,"
         "  vendor TEXT,"
         "  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
         "  updated_at TEXT NOT NULL DEFAULT (datetime('now')));"
         "CREATE TABLE device_credentials ("
         "  device_id TEXT PRIMARY KEY,"
         "  credential_ref TEXT NOT NULL,"       // opaque handle; secret via DPAPI later
         "  FOREIGN KEY(device_id) REFERENCES devices(id) ON DELETE CASCADE);"
         "CREATE TABLE cameras ("
         "  id TEXT PRIMARY KEY,"
         "  device_id TEXT NOT NULL,"
         "  name TEXT NOT NULL,"
         "  main_url TEXT,"
         "  sub_url TEXT,"
         "  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
         "  updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
         "  FOREIGN KEY(device_id) REFERENCES devices(id) ON DELETE CASCADE);"
         "CREATE TABLE camera_groups ("
         "  id TEXT PRIMARY KEY,"
         "  name TEXT NOT NULL);"
         "CREATE TABLE camera_group_members ("
         "  group_id TEXT NOT NULL,"
         "  camera_id TEXT NOT NULL,"
         "  PRIMARY KEY(group_id, camera_id),"
         "  FOREIGN KEY(group_id) REFERENCES camera_groups(id) ON DELETE CASCADE,"
         "  FOREIGN KEY(camera_id) REFERENCES cameras(id) ON DELETE CASCADE);"},

        // v2 — operator settings (key/value) and per-camera stream profiles.
        {2, "operator_settings_and_profiles",
         "CREATE TABLE operator_settings ("
         "  key TEXT PRIMARY KEY,"
         "  value TEXT NOT NULL,"
         "  updated_at TEXT NOT NULL DEFAULT (datetime('now')));"
         "CREATE TABLE stream_profiles ("
         "  camera_id TEXT NOT NULL,"
         "  tier TEXT NOT NULL,"                 // main / sub / thumb
         "  width INTEGER, height INTEGER, codec TEXT,"
         "  PRIMARY KEY(camera_id, tier),"
         "  FOREIGN KEY(camera_id) REFERENCES cameras(id) ON DELETE CASCADE);"},

        // v3 — workspace layout state: the tile count and per-tile operator
        // overrides (desired tier + priority) for each stateful instance
        // (Live / Playback). This is what 5c persists across a restart.
        {3, "workspace_layout",
         "CREATE TABLE workspace_instance ("
         "  name TEXT PRIMARY KEY,"              // 'live' | 'playback'
         "  tile_count INTEGER NOT NULL,"
         "  updated_at TEXT NOT NULL DEFAULT (datetime('now')));"
         "CREATE TABLE workspace_tile ("
         "  instance TEXT NOT NULL,"
         "  tile_id INTEGER NOT NULL,"
         "  desired_tier INTEGER NOT NULL,"      // vms::Tier value (3=Main..0=Off)
         "  priority INTEGER NOT NULL,"          // vms::Priority value
         "  PRIMARY KEY(instance, tile_id),"
         "  FOREIGN KEY(instance) REFERENCES workspace_instance(name) "
         "    ON DELETE CASCADE);"},

        // v4 — recording segment index (native increment 7a). One row per
        // COMPLETED recorded segment; the footage itself lives in files on
        // deployer-chosen storage and this table indexes its availability. This
        // is the OPTIONAL local recording of P0-01E, not a mandatory archive.
        // Times are UTC 'YYYY-MM-DD HH:MM:SS' (SQLite datetime() format) so age
        // math works. There is intentionally NO foreign key to cameras: recorded
        // footage / evidence may outlive a camera's current config row, and its
        // lifecycle is governed by retention (P4-06), not by the inventory.
        {4, "recording_segments",
         "CREATE TABLE segments ("
         "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "  camera_id TEXT NOT NULL,"
         "  start_utc TEXT NOT NULL,"
         "  end_utc TEXT NOT NULL,"
         "  path TEXT NOT NULL,"
         "  codec TEXT,"
         "  bytes INTEGER NOT NULL DEFAULT 0);"
         "CREATE INDEX idx_segments_cam_start ON segments(camera_id, start_utc);"},

        // v5 — device kind (native increment 15, P2-01). Distinguishes the
        // onboarding workflow a device belongs to: a single-channel direct IP
        // 'camera' vs a multi-channel recorder ('nvr' / 'dvr' / 'hybrid'). A
        // forward-only additive column with a default so existing rows (and the
        // v1 direct-camera onboard path) stay valid; the recorder distinction is
        // metadata, not a new table — channels already live in `cameras`.
        {5, "device_kind",
         "ALTER TABLE devices ADD COLUMN kind TEXT NOT NULL DEFAULT 'camera';"},

        // v6 — per-channel disable (native increment 17, P2-05). A disabled
        // channel STAYS in inventory (its stable id + operator name preserved)
        // but is excluded from active use: the default device group holds only
        // enabled channels. Forward-only additive column with a default so every
        // existing channel stays enabled and the v1..v5 onboard paths are valid.
        {6, "camera_disabled",
         "ALTER TABLE cameras ADD COLUMN disabled INTEGER NOT NULL DEFAULT 0;"},

        // v7 — operator channel ordering (native increment 18, P2-05). An
        // explicit sort key so the operator can reorder a device's channels;
        // listing is ORDER BY sort_order, id, so a default of 0 preserves the
        // prior by-id order until the operator rearranges. Forward-only additive.
        {7, "camera_sort_order",
         "ALTER TABLE cameras ADD COLUMN sort_order INTEGER NOT NULL DEFAULT 0;"},

        // v8 — device attach/detach (native increment 21, P2-06). A detached
        // device stays fully configured (rows, channels, credential preserved)
        // but is excluded from active use: its default group empties and the
        // health poll skips it. Forward-only additive, default 0 (attached).
        {8, "device_disabled",
         "ALTER TABLE devices ADD COLUMN disabled INTEGER NOT NULL DEFAULT 0;"},

        // v9 — spatial canvas tile positions (native increment 24, P3-15/P3-01).
        // Each workspace tile can carry an operator-dragged world position on
        // the spatial canvas. Forward-only additive; the -1 default means
        // "unset", so existing layouts fall back to the default grid placement.
        {9, "workspace_tile_pos",
         "ALTER TABLE workspace_tile ADD COLUMN pos_x REAL NOT NULL DEFAULT -1;"
         "ALTER TABLE workspace_tile ADD COLUMN pos_y REAL NOT NULL DEFAULT -1;"},

        // v10 — durable audit history (native increment 28, P1-06). One row per
        // command-envelope attempt, refusals included. Append-only by contract
        // (AuditRepo exposes no update/delete); each row hash-chains to its
        // predecessor so editing or deleting any row breaks the chain from
        // that point (tamper evidence). Forward-only additive.
        {10, "audit_log",
         "CREATE TABLE audit_log ("
         "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
         "  time_utc TEXT NOT NULL,"
         "  source TEXT NOT NULL,"      // 'palette' | 'api' | 'ui'
         "  command TEXT NOT NULL,"
         "  args TEXT NOT NULL,"
         "  outcome TEXT NOT NULL,"
         "  message TEXT NOT NULL,"
         "  prev_hash TEXT NOT NULL,"
         "  row_hash TEXT NOT NULL);"},

        // v11 — the first canonical premises model plus camera coverage
        // metadata (native increment 31, P3-15). A floor owns only metadata for
        // an optional image underlay; image bytes remain outside the database.
        // Facing/FOV are presentation facts and do not alter governor policy.
        {11, "premises_and_camera_fov",
         "CREATE TABLE premises_site ("
         "  id TEXT PRIMARY KEY,"
         "  name TEXT NOT NULL,"
         "  timezone TEXT NOT NULL DEFAULT 'UTC',"
         "  updated_at TEXT NOT NULL DEFAULT (datetime('now')));"
         "CREATE TABLE premises_floor ("
         "  id TEXT PRIMARY KEY,"
         "  site_id TEXT NOT NULL,"
         "  name TEXT NOT NULL,"
         "  plan_uri TEXT NOT NULL DEFAULT '',"
         "  world_width REAL NOT NULL DEFAULT 1600,"
         "  world_height REAL NOT NULL DEFAULT 900,"
         "  updated_at TEXT NOT NULL DEFAULT (datetime('now')) ,"
         "  FOREIGN KEY(site_id) REFERENCES premises_site(id) ON DELETE CASCADE);"
         "CREATE TABLE premises_active ("
         "  workspace TEXT PRIMARY KEY,"
         "  site_id TEXT NOT NULL,"
         "  floor_id TEXT,"
         "  FOREIGN KEY(site_id) REFERENCES premises_site(id) ON DELETE CASCADE,"
         "  FOREIGN KEY(floor_id) REFERENCES premises_floor(id) ON DELETE SET NULL);"
         "ALTER TABLE workspace_tile ADD COLUMN facing_deg REAL NOT NULL DEFAULT 0;"
         "ALTER TABLE workspace_tile ADD COLUMN fov_deg REAL NOT NULL DEFAULT 70;"},

        // v12 — P3-18 operating hours (native increment 36). A marker row
        // distinguishes "configured but closed" from "not configured". Weekly
        // and exception windows are local wall-clock minutes in the site's IANA
        // timezone; date exceptions replace the weekly rule for that date.
        {12, "premises_operating_hours",
         "CREATE TABLE premises_hours_schedule ("
         "  site_id TEXT PRIMARY KEY,"
         "  updated_at TEXT NOT NULL DEFAULT (datetime('now')) ,"
         "  FOREIGN KEY(site_id) REFERENCES premises_site(id) ON DELETE CASCADE);"
         "CREATE TABLE premises_hours_weekly ("
         "  site_id TEXT NOT NULL,"
         "  weekday INTEGER NOT NULL CHECK(weekday BETWEEN 1 AND 7),"
         "  start_minute INTEGER NOT NULL CHECK(start_minute BETWEEN 0 AND 1439),"
         "  end_minute INTEGER NOT NULL CHECK(end_minute BETWEEN 1 AND 1440),"
         "  PRIMARY KEY(site_id,weekday,start_minute,end_minute),"
         "  CHECK(start_minute < end_minute),"
         "  FOREIGN KEY(site_id) REFERENCES premises_hours_schedule(site_id) "
         "    ON DELETE CASCADE);"
         "CREATE TABLE premises_hours_exception ("
         "  site_id TEXT NOT NULL,"
         "  local_date TEXT NOT NULL CHECK(length(local_date)=10),"
         "  closed INTEGER NOT NULL CHECK(closed IN (0,1)),"
         "  label TEXT NOT NULL DEFAULT '',"
         "  PRIMARY KEY(site_id,local_date),"
         "  FOREIGN KEY(site_id) REFERENCES premises_hours_schedule(site_id) "
         "    ON DELETE CASCADE);"
         "CREATE TABLE premises_hours_exception_window ("
         "  site_id TEXT NOT NULL,"
         "  local_date TEXT NOT NULL,"
         "  start_minute INTEGER NOT NULL CHECK(start_minute BETWEEN 0 AND 1439),"
         "  end_minute INTEGER NOT NULL CHECK(end_minute BETWEEN 1 AND 1440),"
         "  PRIMARY KEY(site_id,local_date,start_minute,end_minute),"
         "  CHECK(start_minute < end_minute),"
         "  FOREIGN KEY(site_id,local_date) "
         "    REFERENCES premises_hours_exception(site_id,local_date) "
         "    ON DELETE CASCADE);"},

        // v13 — site-scoped device operations evidence (native increment 37).
        // Ownership is optional and deleting a site unassigns inventory. The
        // observation row records reachability evidence only: last positive
        // seen and the start of the current uninterrupted positive run.
        {13, "device_site_and_reachability_time",
         "ALTER TABLE devices ADD COLUMN site_id TEXT "
         "  REFERENCES premises_site(id) ON DELETE SET NULL;"
         "CREATE INDEX idx_devices_site ON devices(site_id);"
         "CREATE TABLE device_reach_observation ("
         "  device_id TEXT PRIMARY KEY,"
         "  reach_state TEXT NOT NULL CHECK(reach_state IN ('online','offline')) ,"
         "  observed_at_utc TEXT NOT NULL,"
         "  last_seen_utc TEXT,"
         "  online_since_utc TEXT,"
         "  FOREIGN KEY(device_id) REFERENCES devices(id) ON DELETE CASCADE);"},
    };
}

} // namespace vms::persist
