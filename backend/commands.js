const crypto = require("crypto");
const { formatDateTime } = require("./datetime");

const VALID_INTENTS = new Set([
  "OPEN_CAMERA",
  "CLOSE_CAMERA",
  "OPEN_GROUP",
  "CLOSE_GROUP",
  "PAN_TO_ZONE",
  "ZOOM_TO",
  "START_TRACKING",
  "PAUSE_SESSION",
  "RESUME_SESSION",
  "ESCALATE_SESSION",
  "SEARCH",
  "STOP_ALL",
  "SET_MEDIA_POLICY_VISIBILITY"
]);

const VALID_SOURCES = new Set(["gesture", "voice_regex", "voice_agent", "ui_click"]);

function id(prefix) {
  return `${prefix}-${crypto.randomUUID()}`;
}

function createCommand({ intent, target, params = {}, source = "ui_click", confidence, rawText }) {
  if (!VALID_INTENTS.has(intent)) {
    throw new Error(`Unsupported command intent: ${intent}`);
  }
  if (!VALID_SOURCES.has(source)) {
    throw new Error(`Unsupported command source: ${source}`);
  }
  return {
    id: id("cmd"),
    intent,
    target,
    params,
    source,
    confidence,
    rawText,
    createdAt: formatDateTime()
  };
}

class CommandExecutor {
  constructor(db, services = {}) {
    this.db = db;
    this.services = services;
  }

  execute(command) {
    const normalized = createCommand(command);
    const result = this.dispatch(normalized);
    this.log(normalized, result.status, result);
    return result;
  }

  dispatch(command) {
    switch (command.intent) {
      case "OPEN_CAMERA":
        return this.openCamera(command);
      case "CLOSE_CAMERA":
        return this.closeCamera(command);
      case "OPEN_GROUP":
        return this.openGroup(command);
      case "START_TRACKING":
        return this.startTracking(command);
      case "PAUSE_SESSION":
      case "RESUME_SESSION":
      case "ESCALATE_SESSION":
        return this.updateTrackingSession(command);
      case "PAN_TO_ZONE":
      case "ZOOM_TO":
      case "SEARCH":
      case "SET_MEDIA_POLICY_VISIBILITY":
        return { status: "executed", action: command.intent, target: command.target, params: command.params };
      case "STOP_ALL":
        return this.stopAll(command);
      default:
        return { status: "rejected", reason: "Intent has no executor yet" };
    }
  }

  openCamera(command) {
    const camera = this.resolveEntity("camera", command.target);
    if (!camera) return { status: "rejected", reason: "Camera not found", target: command.target };

    const device = this.db.table("devices").find((item) => item.id === camera.deviceId);
    let streamPlan = null;
    if (device && this.services.mediaGuard) {
      streamPlan = this.services.mediaGuard.planOpen({
        camera,
        device,
        paneContext: command.params.paneContext || "focus",
        tileCount: command.params.tileCount || 1,
        isFocused: true,
        isTracking: Boolean(command.params.trackingSessionId)
      });
    }

    return { status: "executed", action: "openCamera", camera, streamPlan };
  }

  closeCamera(command) {
    const camera = this.resolveEntity("camera", command.target);
    if (!camera) return { status: "rejected", reason: "Camera not found", target: command.target };
    return { status: "executed", action: "closeCamera", cameraId: camera.id };
  }

  openGroup(command) {
    const group = this.resolveEntity("group", command.target);
    if (!group) return { status: "rejected", reason: "Group not found", target: command.target };
    return { status: "executed", action: "openGroup", group };
  }

  startTracking(command) {
    const camera = this.resolveEntity("camera", command.target);
    if (!camera) return { status: "rejected", reason: "Camera not found", target: command.target };

    const session = {
      id: id("track"),
      caseType: command.params.caseType || "other",
      status: "active",
      priority: command.params.priority || "normal",
      subjectDescriptor: command.params.subjectDescriptor || "",
      startedAt: formatDateTime(),
      lastActiveAt: formatDateTime()
    };
    this.db.upsert("trackingSessions", session);
    this.addBreadcrumb(session.id, camera.id, command);

    return { status: "executed", action: "startTracking", session, camera };
  }

  updateTrackingSession(command) {
    const session = this.resolveEntity("trackingSession", command.target) || this.latestTrackingSession();
    if (!session) return { status: "rejected", reason: "Tracking session not found", target: command.target };

    const statusByIntent = {
      PAUSE_SESSION: "paused",
      RESUME_SESSION: "active",
      ESCALATE_SESSION: "active"
    };
    const next = {
      ...session,
      status: statusByIntent[command.intent],
      priority: command.intent === "ESCALATE_SESSION" ? "urgent" : session.priority,
      caseType: command.intent === "ESCALATE_SESSION" ? "theft" : session.caseType,
      lastActiveAt: formatDateTime()
    };
    this.db.upsert("trackingSessions", next);
    return { status: "executed", action: command.intent, session: next };
  }

  stopAll() {
    return { status: "needs_confirmation", action: "stopAll", reason: "Bulk close requires operator confirmation" };
  }

  addBreadcrumb(sessionId, cameraId, command) {
    const position = this.db.table("cameraPositions").find((item) => item.cameraId === cameraId);
    this.db.upsert("sessionBreadcrumbs", {
      id: id("crumb"),
      sessionId,
      cameraId,
      enteredAt: formatDateTime(),
      canvasXPct: position ? position.xPct : null,
      canvasYPct: position ? position.yPct : null,
      commandSource: command.source,
      operatorNote: command.params.note || ""
    });
  }

  resolveEntity(type, target = {}) {
    if (!target) return null;
    const tableByType = {
      camera: "cameras",
      group: "groups",
      trackingSession: "trackingSessions"
    };
    const tableName = tableByType[type];
    if (!tableName) return null;
    const rows = this.db.table(tableName);
    if (target.id) return rows.find((item) => item.id === target.id) || null;
    if (!target.name) return null;

    const wanted = normalizeLookup(target.name);
    return rows.find((item) => normalizeLookup(item.displayName || item.name || item.id) === wanted) || null;
  }

  latestTrackingSession() {
    return this.db
      .table("trackingSessions")
      .filter((session) => session.status === "active" || session.status === "paused")
      .sort((a, b) => String(b.lastActiveAt).localeCompare(String(a.lastActiveAt)))[0] || null;
  }

  log(command, status, result) {
    this.db.upsert("commandLog", {
      id: command.id,
      intent: command.intent,
      source: command.source,
      targetType: command.target ? command.target.type : "",
      targetId: command.target ? command.target.id || "" : "",
      targetName: command.target ? command.target.name || "" : "",
      paramsJson: JSON.stringify(command.params || {}),
      confidence: command.confidence,
      rawText: command.rawText || "",
      status,
      resultJson: JSON.stringify(result || {})
    });
  }
}

function normalizeLookup(value) {
  return String(value || "")
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, " ")
    .trim();
}

module.exports = {
  CommandExecutor,
  createCommand,
  normalizeLookup
};
