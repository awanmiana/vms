-- VMS Operator Console - SQLite foundation schema
-- This is an exploratory local/standalone database shape.
-- Production persistence selection and authority remain pending P0-04 approval.
-- Date values should use YYYY-MM-DD.
-- Time values should use HH:MM:SS in 24-hour time.
-- Combined review/query timestamps should use YYYY-MM-DD HH:MM:SS.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS schema_migrations (
  version INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS devices (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL UNIQUE,
  type TEXT NOT NULL CHECK (type IN ('NVR', 'DVR', 'Hybrid DVR', 'IP Camera Direct')),
  vendor TEXT,
  host TEXT NOT NULL,
  port INTEGER NOT NULL,
  channel_count INTEGER NOT NULL DEFAULT 1,
  status TEXT NOT NULL DEFAULT 'unknown',
  max_concurrent_mainstream INTEGER NOT NULL DEFAULT 4,
  max_concurrent_substream INTEGER NOT NULL DEFAULT 32,
  notes TEXT,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS device_credentials (
  device_id TEXT PRIMARY KEY REFERENCES devices(id) ON DELETE CASCADE,
  username TEXT NOT NULL,
  credential_ref TEXT NOT NULL,
  credential_provider TEXT NOT NULL DEFAULT 'windows-dpapi',
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS cameras (
  id TEXT PRIMARY KEY,
  device_id TEXT NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
  channel_number INTEGER NOT NULL,
  display_name TEXT NOT NULL,
  area TEXT NOT NULL DEFAULT 'Unassigned',
  floor TEXT NOT NULL DEFAULT 'Unknown',
  direction TEXT NOT NULL DEFAULT 'Direction not set',
  status TEXT NOT NULL DEFAULT 'unknown',
  discovered INTEGER NOT NULL DEFAULT 0,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE (device_id, channel_number)
);

CREATE TABLE IF NOT EXISTS camera_tags (
  camera_id TEXT NOT NULL REFERENCES cameras(id) ON DELETE CASCADE,
  tag TEXT NOT NULL,
  PRIMARY KEY (camera_id, tag)
);

CREATE TABLE IF NOT EXISTS entities (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL UNIQUE,
  alias TEXT NOT NULL UNIQUE,
  notes TEXT,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS locations (
  id TEXT PRIMARY KEY,
  entity_id TEXT NOT NULL REFERENCES entities(id) ON DELETE CASCADE,
  name TEXT NOT NULL,
  location_type TEXT,
  notes TEXT,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE (entity_id, name)
);

CREATE TABLE IF NOT EXISTS camera_locations (
  camera_id TEXT NOT NULL REFERENCES cameras(id) ON DELETE CASCADE,
  location_id TEXT NOT NULL REFERENCES locations(id) ON DELETE CASCADE,
  coverage_role TEXT NOT NULL DEFAULT 'primary' CHECK (coverage_role IN ('primary', 'secondary', 'partial')),
  notes TEXT,
  PRIMARY KEY (camera_id, location_id)
);

CREATE TABLE IF NOT EXISTS compliance_types (
  id TEXT PRIMARY KEY,
  label TEXT NOT NULL UNIQUE,
  normalized_label TEXT NOT NULL UNIQUE,
  category TEXT NOT NULL CHECK (category IN ('compliance', 'non_compliance')),
  created_via TEXT NOT NULL DEFAULT 'preset' CHECK (created_via IN ('preset', 'operator_freeform', 'whatsapp_import')),
  review_status TEXT NOT NULL DEFAULT 'approved' CHECK (review_status IN ('approved', 'needs_review', 'merged', 'archived')),
  merged_into_type_id TEXT REFERENCES compliance_types(id) ON DELETE SET NULL,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS tag_index (
  id TEXT PRIMARY KEY,
  tag_type TEXT NOT NULL CHECK (tag_type IN ('entity', 'location', 'compliance_type', 'camera', 'custom')),
  reference_id TEXT,
  label TEXT NOT NULL,
  normalized_label TEXT NOT NULL,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE (tag_type, normalized_label, reference_id)
);

CREATE TABLE IF NOT EXISTS compliance_events (
  id TEXT PRIMARY KEY,
  entity_id TEXT NOT NULL REFERENCES entities(id) ON DELETE CASCADE,
  location_id TEXT REFERENCES locations(id) ON DELETE SET NULL,
  compliance_type_id TEXT NOT NULL REFERENCES compliance_types(id) ON DELETE RESTRICT,
  reported_at TEXT NOT NULL CHECK (reported_at GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]'),
  source TEXT NOT NULL CHECK (source IN ('whatsapp', 'operator_manual', 'import')),
  whatsapp_message_id TEXT,
  operator_id TEXT REFERENCES users(id) ON DELETE SET NULL,
  notes TEXT,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS compliance_logs (
  id TEXT PRIMARY KEY,
  entity_id TEXT NOT NULL REFERENCES entities(id) ON DELETE CASCADE,
  entity_name_snapshot TEXT NOT NULL,
  compliance_type_id TEXT NOT NULL REFERENCES compliance_types(id) ON DELETE RESTRICT,
  compliance_type_label_snapshot TEXT NOT NULL,
  review_start_at TEXT NOT NULL CHECK (review_start_at GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]'),
  review_end_at TEXT NOT NULL CHECK (review_end_at GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]'),
  logged_by_user_id TEXT REFERENCES users(id) ON DELETE SET NULL,
  source_whatsapp_message_id TEXT,
  manager_response_whatsapp_message_id TEXT,
  manager_response_text TEXT,
  notes TEXT,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS compliance_log_cameras (
  compliance_log_id TEXT NOT NULL REFERENCES compliance_logs(id) ON DELETE CASCADE,
  camera_id TEXT REFERENCES cameras(id) ON DELETE SET NULL,
  camera_display_name_snapshot TEXT NOT NULL,
  device_name_snapshot TEXT,
  channel_number_snapshot INTEGER,
  PRIMARY KEY (compliance_log_id, camera_display_name_snapshot)
);

CREATE TABLE IF NOT EXISTS compliance_log_locations (
  compliance_log_id TEXT NOT NULL REFERENCES compliance_logs(id) ON DELETE CASCADE,
  location_id TEXT REFERENCES locations(id) ON DELETE SET NULL,
  location_name_snapshot TEXT NOT NULL,
  PRIMARY KEY (compliance_log_id, location_name_snapshot)
);

CREATE TABLE IF NOT EXISTS operator_settings (
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS stream_profiles (
  id TEXT PRIMARY KEY,
  camera_id TEXT NOT NULL REFERENCES cameras(id) ON DELETE CASCADE,
  tier TEXT NOT NULL CHECK (tier IN ('thumb', 'sub', 'main')),
  resolution TEXT,
  fps INTEGER,
  bitrate_kbps INTEGER NOT NULL,
  codec TEXT,
  -- Exploratory protocol-specific placeholder, not the vendor-neutral core contract.
  -- Revisit as a namespaced external profile identifier under P1-04/P2-05.
  interoperability_profile_token TEXT,
  is_available INTEGER NOT NULL DEFAULT 1,
  UNIQUE (camera_id, tier)
);

CREATE TABLE IF NOT EXISTS camera_routes (
  from_camera_id TEXT NOT NULL REFERENCES cameras(id) ON DELETE CASCADE,
  to_camera_id TEXT NOT NULL REFERENCES cameras(id) ON DELETE CASCADE,
  relation TEXT NOT NULL CHECK (relation IN ('previous', 'next', 'nearby')),
  PRIMARY KEY (from_camera_id, to_camera_id, relation)
);

CREATE TABLE IF NOT EXISTS camera_edges (
  id TEXT PRIMARY KEY,
  from_camera_id TEXT NOT NULL REFERENCES cameras(id) ON DELETE CASCADE,
  from_exit_zone TEXT NOT NULL,
  to_camera_id TEXT NOT NULL REFERENCES cameras(id) ON DELETE CASCADE,
  to_entry_zone TEXT NOT NULL,
  walk_time_seconds INTEGER,
  confidence REAL NOT NULL DEFAULT 1.0,
  bidirectional INTEGER NOT NULL DEFAULT 0,
  notes TEXT
);

CREATE TABLE IF NOT EXISTS site_canvases (
  id TEXT PRIMARY KEY,
  premises_id TEXT NOT NULL DEFAULT 'default',
  entity_id TEXT REFERENCES entities(id) ON DELETE SET NULL,
  name TEXT NOT NULL,
  background_image_path TEXT,
  width_px INTEGER,
  height_px INTEGER,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS camera_positions (
  id TEXT PRIMARY KEY,
  camera_id TEXT NOT NULL REFERENCES cameras(id) ON DELETE CASCADE,
  canvas_id TEXT NOT NULL REFERENCES site_canvases(id) ON DELETE CASCADE,
  x_pct REAL NOT NULL CHECK (x_pct >= 0 AND x_pct <= 100),
  y_pct REAL NOT NULL CHECK (y_pct >= 0 AND y_pct <= 100),
  facing_degrees REAL,
  fov_degrees REAL NOT NULL DEFAULT 90,
  notes TEXT,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE (camera_id, canvas_id)
);

CREATE TABLE IF NOT EXISTS tracking_sessions (
  id TEXT PRIMARY KEY,
  case_type TEXT NOT NULL CHECK (case_type IN ('lost_found', 'theft', 'compliance', 'other')),
  status TEXT NOT NULL DEFAULT 'active' CHECK (status IN ('active', 'paused', 'resolved')),
  priority TEXT NOT NULL DEFAULT 'normal' CHECK (priority IN ('normal', 'urgent')),
  subject_descriptor TEXT,
  reported_by TEXT,
  started_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  last_active_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  resolved_at TEXT,
  linked_whatsapp_message_id TEXT
);

CREATE TABLE IF NOT EXISTS session_breadcrumbs (
  id TEXT PRIMARY KEY,
  session_id TEXT NOT NULL REFERENCES tracking_sessions(id) ON DELETE CASCADE,
  camera_id TEXT NOT NULL REFERENCES cameras(id) ON DELETE CASCADE,
  entered_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  exited_at TEXT,
  thumbnail_path TEXT,
  edge_id_used TEXT REFERENCES camera_edges(id) ON DELETE SET NULL,
  canvas_x_pct REAL,
  canvas_y_pct REAL,
  command_source TEXT CHECK (command_source IN ('gesture', 'voice_regex', 'voice_agent', 'ui_click')),
  operator_note TEXT
);

CREATE TABLE IF NOT EXISTS voice_aliases (
  id TEXT PRIMARY KEY,
  entity_type TEXT NOT NULL CHECK (entity_type IN ('camera', 'group', 'zone', 'tag', 'entity', 'location', 'compliance_type', 'ticket')),
  entity_id TEXT NOT NULL,
  alias TEXT NOT NULL,
  normalized_alias TEXT NOT NULL,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  UNIQUE (entity_type, normalized_alias)
);

CREATE TABLE IF NOT EXISTS command_log (
  id TEXT PRIMARY KEY,
  intent TEXT NOT NULL,
  source TEXT NOT NULL CHECK (source IN ('gesture', 'voice_regex', 'voice_agent', 'ui_click')),
  target_type TEXT,
  target_id TEXT,
  target_name TEXT,
  params_json TEXT,
  confidence REAL,
  raw_text TEXT,
  status TEXT NOT NULL CHECK (status IN ('accepted', 'needs_confirmation', 'rejected', 'executed', 'failed')),
  result_json TEXT,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS camera_groups (
  id TEXT PRIMARY KEY,
  name TEXT NOT NULL,
  purpose TEXT,
  preferred_grid INTEGER NOT NULL DEFAULT 4,
  device_id TEXT REFERENCES devices(id) ON DELETE CASCADE,
  is_system INTEGER NOT NULL DEFAULT 0,
  notes TEXT
);

CREATE TABLE IF NOT EXISTS camera_group_members (
  group_id TEXT NOT NULL REFERENCES camera_groups(id) ON DELETE CASCADE,
  camera_id TEXT NOT NULL REFERENCES cameras(id) ON DELETE CASCADE,
  sort_order INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (group_id, camera_id)
);

-- Every physical device owns one deterministic, system-managed camera group.
-- These backfills make the rule true for databases created before the triggers
-- below were introduced.
INSERT OR IGNORE INTO camera_groups (
  id,
  name,
  purpose,
  preferred_grid,
  device_id,
  is_system,
  notes
)
SELECT
  'grp-device-' || id,
  name || ' (Assigned)',
  'Auto-assigned device channels',
  4,
  id,
  1,
  'Created automatically from this device''s channels.'
FROM devices;

INSERT OR IGNORE INTO camera_group_members (group_id, camera_id, sort_order)
SELECT 'grp-device-' || device_id, id, channel_number
FROM cameras;

CREATE TRIGGER IF NOT EXISTS devices_create_system_group
AFTER INSERT ON devices
BEGIN
  INSERT OR IGNORE INTO camera_groups (
    id,
    name,
    purpose,
    preferred_grid,
    device_id,
    is_system,
    notes
  ) VALUES (
    'grp-device-' || NEW.id,
    NEW.name || ' (Assigned)',
    'Auto-assigned device channels',
    4,
    NEW.id,
    1,
    'Created automatically from this device''s channels.'
  );
END;

CREATE TRIGGER IF NOT EXISTS devices_rename_system_group
AFTER UPDATE OF name ON devices
BEGIN
  UPDATE camera_groups
  SET name = NEW.name || ' (Assigned)'
  WHERE device_id = NEW.id AND is_system = 1;
END;

CREATE TRIGGER IF NOT EXISTS cameras_add_to_system_group
AFTER INSERT ON cameras
BEGIN
  INSERT OR IGNORE INTO camera_groups (
    id,
    name,
    purpose,
    preferred_grid,
    device_id,
    is_system,
    notes
  )
  SELECT
    'grp-device-' || id,
    name || ' (Assigned)',
    'Auto-assigned device channels',
    4,
    id,
    1,
    'Created automatically from this device''s channels.'
  FROM devices
  WHERE id = NEW.device_id;

  INSERT OR IGNORE INTO camera_group_members (group_id, camera_id, sort_order)
  VALUES ('grp-device-' || NEW.device_id, NEW.id, NEW.channel_number);
END;

CREATE TRIGGER IF NOT EXISTS cameras_move_between_system_groups
AFTER UPDATE OF device_id, channel_number ON cameras
BEGIN
  DELETE FROM camera_group_members
  WHERE group_id = 'grp-device-' || OLD.device_id AND camera_id = OLD.id;

  INSERT OR IGNORE INTO camera_groups (
    id,
    name,
    purpose,
    preferred_grid,
    device_id,
    is_system,
    notes
  )
  SELECT
    'grp-device-' || id,
    name || ' (Assigned)',
    'Auto-assigned device channels',
    4,
    id,
    1,
    'Created automatically from this device''s channels.'
  FROM devices
  WHERE id = NEW.device_id;

  INSERT OR IGNORE INTO camera_group_members (group_id, camera_id, sort_order)
  VALUES ('grp-device-' || NEW.device_id, NEW.id, NEW.channel_number);
END;

CREATE TABLE IF NOT EXISTS users (
  id TEXT PRIMARY KEY,
  full_name TEXT NOT NULL,
  username TEXT NOT NULL UNIQUE,
  role TEXT NOT NULL,
  status TEXT NOT NULL CHECK (status IN ('Active', 'Suspended', 'Revoked')),
  notes TEXT,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS user_group_access (
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  group_id TEXT NOT NULL REFERENCES camera_groups(id) ON DELETE CASCADE,
  PRIMARY KEY (user_id, group_id)
);

CREATE TABLE IF NOT EXISTS user_permissions (
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  permission TEXT NOT NULL,
  PRIMARY KEY (user_id, permission)
);

CREATE TABLE IF NOT EXISTS user_sessions (
  id TEXT PRIMARY KEY,
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  issued_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  expires_at TEXT NOT NULL,
  revoked_at TEXT,
  workstation_id TEXT,
  session_budget_kbps INTEGER NOT NULL DEFAULT 15000,
  network_profile TEXT NOT NULL DEFAULT 'lan' CHECK (network_profile IN ('lan', 'remote'))
);

CREATE TABLE IF NOT EXISTS camera_sessions (
  id TEXT PRIMARY KEY,
  user_session_id TEXT NOT NULL REFERENCES user_sessions(id) ON DELETE CASCADE,
  camera_id TEXT NOT NULL REFERENCES cameras(id) ON DELETE CASCADE,
  device_id TEXT NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
  tier TEXT NOT NULL CHECK (tier IN ('thumb', 'sub', 'main', 'paused')),
  pane_context TEXT NOT NULL CHECK (pane_context IN ('grid', 'focus', 'tracking', 'playback')),
  bitrate_kbps INTEGER NOT NULL DEFAULT 0,
  opened_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  last_active_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  status TEXT NOT NULL DEFAULT 'opening'
);

CREATE TABLE IF NOT EXISTS incidents (
  id TEXT PRIMARY KEY,
  source TEXT NOT NULL CHECK (source IN ('Live', 'Playback', 'Manual')),
  camera_id TEXT REFERENCES cameras(id) ON DELETE SET NULL,
  occurred_at TEXT NOT NULL,
  issue_type TEXT NOT NULL,
  note TEXT NOT NULL,
  status TEXT NOT NULL CHECK (status IN ('Draft', 'Ready', 'Sent', 'Closed')),
  created_by_user_id TEXT REFERENCES users(id) ON DELETE SET NULL,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS tickets (
  id TEXT PRIMARY KEY,
  entity_id TEXT REFERENCES entities(id) ON DELETE SET NULL,
  location_id TEXT REFERENCES locations(id) ON DELETE SET NULL,
  query_type TEXT NOT NULL DEFAULT 'review',
  status TEXT NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'in_progress', 'resolved', 'escalated')),
  raw_message_text TEXT NOT NULL,
  requested_start_time TEXT CHECK (requested_start_time IS NULL OR requested_start_time GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]'),
  requested_end_time TEXT CHECK (requested_end_time IS NULL OR requested_end_time GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]'),
  subject_descriptor TEXT,
  linked_tracking_session_id TEXT REFERENCES tracking_sessions(id) ON DELETE SET NULL,
  whatsapp_message_id TEXT,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  resolved_at TEXT
);

CREATE TABLE IF NOT EXISTS device_discovery_results (
  id TEXT PRIMARY KEY,
  vendor TEXT NOT NULL,
  host TEXT NOT NULL,
  port INTEGER NOT NULL,
  name TEXT NOT NULL,
  status TEXT NOT NULL DEFAULT 'candidate',
  discovered_at TEXT NOT NULL,
  raw_json TEXT NOT NULL DEFAULT '{}'
);

CREATE TABLE IF NOT EXISTS device_health_checks (
  id TEXT PRIMARY KEY,
  device_id TEXT REFERENCES devices(id) ON DELETE CASCADE,
  checked_at TEXT NOT NULL,
  status TEXT NOT NULL CHECK (status IN ('online', 'offline', 'warning', 'unknown')),
  latency_ms INTEGER,
  firmware_version TEXT,
  message TEXT,
  raw_json TEXT NOT NULL DEFAULT '{}'
);

CREATE TABLE IF NOT EXISTS device_events (
  id TEXT PRIMARY KEY,
  device_id TEXT REFERENCES devices(id) ON DELETE CASCADE,
  camera_id TEXT REFERENCES cameras(id) ON DELETE SET NULL,
  event_type TEXT NOT NULL,
  severity TEXT NOT NULL DEFAULT 'info' CHECK (severity IN ('info', 'warning', 'critical')),
  occurred_at TEXT NOT NULL,
  title TEXT NOT NULL,
  message TEXT,
  acknowledged_at TEXT,
  raw_json TEXT NOT NULL DEFAULT '{}'
);

CREATE TABLE IF NOT EXISTS ptz_command_log (
  id TEXT PRIMARY KEY,
  device_id TEXT NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
  camera_id TEXT NOT NULL REFERENCES cameras(id) ON DELETE CASCADE,
  command TEXT NOT NULL,
  params_json TEXT NOT NULL DEFAULT '{}',
  status TEXT NOT NULL CHECK (status IN ('queued', 'sent', 'rejected', 'failed')),
  message TEXT,
  created_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_cameras_device_channel ON cameras(device_id, channel_number);
CREATE INDEX IF NOT EXISTS idx_camera_sessions_device_tier ON camera_sessions(device_id, tier);
CREATE INDEX IF NOT EXISTS idx_incidents_status_time ON incidents(status, occurred_at);
CREATE INDEX IF NOT EXISTS idx_locations_entity_name ON locations(entity_id, name);
CREATE INDEX IF NOT EXISTS idx_camera_locations_location ON camera_locations(location_id, camera_id);
CREATE INDEX IF NOT EXISTS idx_tag_index_lookup ON tag_index(normalized_label, tag_type);
CREATE INDEX IF NOT EXISTS idx_compliance_events_location_time ON compliance_events(location_id, reported_at);
CREATE INDEX IF NOT EXISTS idx_compliance_events_type_time ON compliance_events(compliance_type_id, reported_at);
CREATE INDEX IF NOT EXISTS idx_compliance_logs_entity_time ON compliance_logs(entity_id, review_start_at, review_end_at);
CREATE INDEX IF NOT EXISTS idx_compliance_log_cameras_camera ON compliance_log_cameras(camera_id);
CREATE INDEX IF NOT EXISTS idx_compliance_log_locations_location ON compliance_log_locations(location_id);
CREATE INDEX IF NOT EXISTS idx_tickets_status_created ON tickets(status, created_at);
CREATE INDEX IF NOT EXISTS idx_tickets_location_time ON tickets(location_id, requested_start_time, requested_end_time);
CREATE INDEX IF NOT EXISTS idx_camera_edges_from ON camera_edges(from_camera_id, from_exit_zone, confidence);
CREATE INDEX IF NOT EXISTS idx_camera_positions_canvas ON camera_positions(canvas_id, x_pct, y_pct);
CREATE INDEX IF NOT EXISTS idx_tracking_sessions_status ON tracking_sessions(status, priority, last_active_at);
CREATE INDEX IF NOT EXISTS idx_session_breadcrumbs_session ON session_breadcrumbs(session_id, entered_at);
CREATE INDEX IF NOT EXISTS idx_voice_aliases_lookup ON voice_aliases(entity_type, normalized_alias);
CREATE INDEX IF NOT EXISTS idx_command_log_created ON command_log(created_at, source, intent);
CREATE INDEX IF NOT EXISTS idx_device_discovery_host ON device_discovery_results(host, port);
CREATE INDEX IF NOT EXISTS idx_device_health_device_time ON device_health_checks(device_id, checked_at);
CREATE INDEX IF NOT EXISTS idx_device_events_time ON device_events(occurred_at, severity);
CREATE INDEX IF NOT EXISTS idx_ptz_command_camera_time ON ptz_command_log(camera_id, created_at);
