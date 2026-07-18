const crypto = require("crypto");
const { formatDateTime } = require("./datetime");
const {
  DeviceAdapterRegistry,
  createDefaultDeviceAdapterRegistry
} = require("./device-adapters");

const DEVICE_OPERATIONS = Object.freeze({
  DISCOVERY: "device.discovery",
  HEALTH: "device.health",
  FIRMWARE: "device.firmware",
  EVENTS: "device.events",
  PTZ: "camera.ptz"
});

function id(prefix) {
  return `${prefix}-${crypto.randomUUID()}`;
}

function explicitAdapterId(deviceOrQuery = {}) {
  if (typeof deviceOrQuery === "string") return deviceOrQuery;
  return deviceOrQuery?.adapterId || "";
}

class DeviceIntegrationService {
  constructor(db, registry = createDefaultDeviceAdapterRegistry()) {
    if (!(registry instanceof DeviceAdapterRegistry)) {
      throw new TypeError("DeviceIntegrationService requires a DeviceAdapterRegistry.");
    }
    this.db = db;
    this.registry = registry;
  }

  adapterFor(deviceOrQuery = {}) {
    return this.registry.resolve(explicitAdapterId(deviceOrQuery));
  }

  async discover({ range = "", adapterId = "" } = {}) {
    const outcome = await this.registry.execute(adapterId, DEVICE_OPERATIONS.DISCOVERY, { range });
    if (outcome.status !== "succeeded") return { ...outcome, devices: [] };

    const discovered = Array.isArray(outcome.data) ? outcome.data : [];
    discovered.forEach((device) => {
      const port = Number(device.port);
      this.db.upsert("deviceDiscoveryResults", {
        id: device.id || id("disc"),
        adapterId: outcome.adapterId,
        vendor: device.vendor || "",
        host: device.host,
        port: Number.isInteger(port) && port >= 1 && port <= 65535 ? port : null,
        name: device.name || device.host,
        status: device.status || "candidate",
        discoveredAt: formatDateTime(),
        rawJson: JSON.stringify(device)
      });
    });
    return { ...outcome, devices: discovered };
  }

  async testConnection(device) {
    const outcome = await this.registry.execute(
      explicitAdapterId(device),
      DEVICE_OPERATIONS.HEALTH,
      { device }
    );
    if (outcome.status !== "succeeded") return { ...outcome, health: null };

    const result = outcome.data || {};
    const row = {
      id: id("health"),
      deviceId: device.id,
      checkedAt: formatDateTime(),
      status: result.ok ? "online" : "offline",
      latencyMs: result.latencyMs ?? null,
      firmwareVersion: result.firmwareVersion || "",
      message: result.message || "",
      rawJson: JSON.stringify(result)
    };
    this.db.upsert("deviceHealthChecks", row);
    return { ...outcome, health: row };
  }

  async inspectFirmware(device) {
    const outcome = await this.registry.execute(
      explicitAdapterId(device),
      DEVICE_OPERATIONS.FIRMWARE,
      { device }
    );
    if (outcome.status !== "succeeded") return { ...outcome, firmware: null };

    const result = outcome.data || {};
    return {
      ...outcome,
      firmware: {
        deviceId: device.id,
        checkedAt: formatDateTime(),
        currentVersion: result.currentVersion || "",
        latestKnownVersion: result.latestKnownVersion || "",
        updateAvailable: Boolean(result.updateAvailable),
        advisory: result.advisory || ""
      }
    };
  }

  async ingestEvents(device, { since } = {}) {
    const outcome = await this.registry.execute(
      explicitAdapterId(device),
      DEVICE_OPERATIONS.EVENTS,
      { device, since }
    );
    if (outcome.status !== "succeeded") return { ...outcome, events: [] };

    const events = Array.isArray(outcome.data) ? outcome.data : [];
    events.forEach((event) => {
      this.db.upsert("deviceEvents", {
        id: event.id || id("event"),
        deviceId: device.id,
        cameraId: event.cameraId || "",
        eventType: event.eventType || "device",
        severity: event.severity || "info",
        occurredAt: event.occurredAt || formatDateTime(),
        title: event.title || "Device event",
        message: event.message || "",
        acknowledgedAt: "",
        rawJson: JSON.stringify(event)
      });
    });
    return { ...outcome, events };
  }

  async executePtzCommand(device, camera, command) {
    const outcome = await this.registry.execute(
      explicitAdapterId(device),
      DEVICE_OPERATIONS.PTZ,
      { device, camera, command }
    );

    if (outcome.outcomeUnknown === true) {
      const commandRecord = this.recordPtzOutcome(device, camera, command, {
        status: "outcome_unknown",
        message: outcome.message,
        reasonCode: outcome.reasonCode
      });
      return { ...outcome, commandRecord };
    }

    if (outcome.status !== "succeeded") return { ...outcome, commandRecord: null };

    const result = outcome.data || {};
    const commandRecord = this.recordPtzOutcome(device, camera, command, {
      status: result.ok ? "succeeded" : "rejected",
      message: result.message || ""
    });
    return { ...outcome, commandRecord };
  }

  // Backward-compatible name for existing callers. PTZ is still executed
  // immediately and is never placed into a delayed command queue.
  async queuePtzCommand(device, camera, command) {
    return this.executePtzCommand(device, camera, command);
  }

  recordPtzOutcome(device, camera, command, { status, message = "", reasonCode = "" }) {
    const row = {
      id: id("ptz"),
      deviceId: device.id,
      cameraId: camera.id,
      command: command.action,
      paramsJson: JSON.stringify(command.params || {}),
      status,
      message,
      reasonCode,
      executionMode: "live",
      replayAllowed: false,
      createdAt: formatDateTime()
    };
    this.db.upsert("ptzCommandLog", row);
    return row;
  }
}

module.exports = {
  DEVICE_OPERATIONS,
  DeviceIntegrationService,
  explicitAdapterId
};
