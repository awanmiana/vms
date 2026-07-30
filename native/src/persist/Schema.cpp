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
    };
}

} // namespace vms::persist
