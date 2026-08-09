const { createId } = require("../shared/core");
const { formatDateTime } = require("./datetime");
const {
  DeviceAdapterRegistry,
  createDefaultDeviceAdapterRegistry
} = require("./device-adapters");
const {
  OPERATION_IDS,
  ResiliencePolicyService,
  fromLegacyStatus,
  policyOutcome
} = require("./resilience");

const DEVICE_OPERATIONS = Object.freeze({
  DISCOVERY: OPERATION_IDS.DEVICE_DISCOVERY,
  HEALTH: OPERATION_IDS.DEVICE_HEALTH,
  FIRMWARE: OPERATION_IDS.DEVICE_FIRMWARE,
  EVENTS: OPERATION_IDS.DEVICE_EVENTS,
  PTZ: OPERATION_IDS.PTZ
});

function explicitAdapterId(deviceOrQuery = {}) {
  if (typeof deviceOrQuery === "string") return deviceOrQuery;
  return deviceOrQuery?.adapterId || "";
}

class DeviceIntegrationService {
  constructor(db, registry = createDefaultDeviceAdapterRegistry(), {
    resiliencePolicy = new ResiliencePolicyService()
  } = {}) {
    if (!(registry instanceof DeviceAdapterRegistry)) {
      throw new TypeError("DeviceIntegrationService requires a DeviceAdapterRegistry.");
    }
    this.db = db;
    this.registry = registry;
    this.resiliencePolicy = resiliencePolicy;
  }

  adapterFor(deviceOrQuery = {}) {
    return this.registry.resolve(explicitAdapterId(deviceOrQuery));
  }

  async executeAdapter(adapterId, operationId, adapterContext, policyContext = {}) {
    const preflight = this.resiliencePolicy.operationPreflight(operationId, {
      authorizationAvailable: true,
      durableAuditAvailable: !this.db.persistenceFallback,
      ...policyContext
    });
    if (!preflight.allowed) {
      return {
        status: "failed",
        outcome: policyOutcome(preflight),
        adapterId: explicitAdapterId(adapterId),
        operation: operationId,
        reasonCode: preflight.reasonCode,
        message: `Operation ${operationId} was blocked by resilience policy.`,
        policyDecision: preflight
      };
    }
    return this.registry.execute(explicitAdapterId(adapterId), operationId, adapterContext);
  }

  async discover({ range = "", adapterId = "" } = {}) {
    const outcome = await this.executeAdapter(adapterId, DEVICE_OPERATIONS.DISCOVERY, { range });
    if (outcome.status !== "succeeded") return { ...outcome, devices: [] };

    const discovered = Array.isArray(outcome.data) ? outcome.data : [];
    discovered.forEach((device) => {
      const port = Number(device.port);
      this.db.upsert("deviceDiscoveryResults", {
        id: device.id || createId("disc"),
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
    const outcome = await this.executeAdapter(
      device,
      DEVICE_OPERATIONS.HEALTH,
      { device }
    );
    if (outcome.status !== "succeeded") return { ...outcome, health: null };

    const result = outcome.data || {};
    const observedAt = formatDateTime();
    const dimensions = fromLegacyStatus(
      result.ok === true ? "online" : result.ok === false ? "offline" : "unknown",
      {
        resourceId: device.id,
        adapter: "available",
        lastObservedAt: observedAt,
        message: result.message || ""
      }
    );
    const row = {
      id: createId("health"),
      deviceId: device.id,
      checkedAt: observedAt,
      status: result.ok === true ? "online" : result.ok === false ? "offline" : "unknown",
      reachability: dimensions.reachability,
      freshness: dimensions.freshness,
      functional: dimensions.functional,
      adapter: dimensions.adapter,
      latencyMs: result.latencyMs ?? null,
      firmwareVersion: result.firmwareVersion || "",
      message: result.message || "",
      rawJson: JSON.stringify(result)
    };
    this.db.upsert("deviceHealthChecks", row);
    return { ...outcome, health: row };
  }

  async inspectFirmware(device) {
    const outcome = await this.executeAdapter(
      device,
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
    const outcome = await this.executeAdapter(
      device,
      DEVICE_OPERATIONS.EVENTS,
      { device, since }
    );
    if (outcome.status !== "succeeded") return { ...outcome, events: [] };

    const events = Array.isArray(outcome.data) ? outcome.data : [];
    events.forEach((event) => {
      this.db.upsert("deviceEvents", {
        id: event.id || createId("event"),
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
    const outcome = await this.executeAdapter(
      device,
      DEVICE_OPERATIONS.PTZ,
      { device, camera, command },
      {
        now: command.now,
        expiresAt: command.expiresAt,
        authorizationAvailable: command.authorizationAvailable !== false,
        durableAuditAvailable: command.durableAuditAvailable ?? !this.db.persistenceFallback
      }
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
      id: createId("ptz"),
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
