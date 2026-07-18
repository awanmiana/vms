(function operationPolicyModule(root, factory) {
  const contract = typeof module === "object" && module.exports
    ? require("./contract")
    : root.VmsResilienceContract;
  const api = factory(contract);
  if (typeof module === "object" && module.exports) {
    module.exports = api;
  } else {
    root.VmsResilienceOperations = api;
  }
})(typeof globalThis !== "undefined" ? globalThis : this, function createOperationPolicy(contract) {
  "use strict";

  const {
    FAILURE_POLICY_VERSION,
    OPERATION_SAFETY_CLASSES,
    QUEUE_MODES,
    RETRY_MODES
  } = contract;

  const OPERATION_OUTCOMES = Object.freeze([
    "allowed",
    "blocked",
    "expired",
    "outcome-unknown"
  ]);

  const OPERATION_IDS = Object.freeze({
    INVENTORY_READ: "inventory.read",
    INVENTORY_WRITE: "inventory.write",
    DEVICE_DISCOVERY: "device.discovery",
    DEVICE_HEALTH: "device.health",
    DEVICE_EVENTS: "device.events",
    LIVE_VIEW: "media.live-view",
    PLAYBACK: "media.playback",
    DOWNLOAD: "media.download",
    ALARM_ACKNOWLEDGE: "alarm.acknowledge",
    AUDIT_APPEND: "audit.append",
    PTZ: "camera.ptz",
    ACCESS_UNLOCK: "access.unlock",
    BARRIER_OPEN: "access.barrier-open",
    RELAY_ACTIVATE: "device.relay-activate",
    BROADCAST: "device.broadcast",
    REBOOT: "device.reboot",
    SHUTDOWN: "device.shutdown",
    FORMAT_STORAGE: "storage.format",
    DELETE_RECORDING: "recording.delete",
    CHANGE_PASSWORD: "device.password-change",
    UPDATE_FIRMWARE: "device.firmware-update"
  });

  class OperationPolicyError extends Error {
    constructor(code, message) {
      super(message);
      this.name = "OperationPolicyError";
      this.code = code;
    }
  }

  function normalizeOperationPolicy(policy) {
    if (!policy || typeof policy !== "object" || Array.isArray(policy)) {
      throw new OperationPolicyError("INVALID_OPERATION_POLICY", "Operation policy must be an object.");
    }
    const id = String(policy.id || "").trim().toLowerCase();
    if (!/^[a-z][a-z0-9-]*(?:\.[a-z][a-z0-9-]*)+$/.test(id)) {
      throw new OperationPolicyError("INVALID_OPERATION_ID", `Invalid operation id: ${policy.id || "(empty)"}`);
    }
    const safetyClass = OPERATION_SAFETY_CLASSES.includes(policy.safetyClass)
      ? policy.safetyClass
      : "restricted";
    const retryMode = RETRY_MODES.includes(policy.retryMode) ? policy.retryMode : "never";
    const queueMode = QUEUE_MODES.includes(policy.queueMode) ? policy.queueMode : "none";
    const physicalOrDestructive = safetyClass === "ephemeral-physical" || safetyClass === "destructive";
    return Object.freeze({
      version: FAILURE_POLICY_VERSION,
      id,
      safetyClass,
      retryMode,
      queueMode: physicalOrDestructive ? "none" : queueMode,
      mayReplay: physicalOrDestructive ? false : Boolean(policy.mayReplay),
      requiresExpiry: physicalOrDestructive || Boolean(policy.requiresExpiry),
      requiresDurableAudit: Boolean(policy.requiresDurableAudit),
      ambiguousDeliveryIsUnknown: physicalOrDestructive || Boolean(policy.ambiguousDeliveryIsUnknown),
      description: String(policy.description || "").trim()
    });
  }

  class OperationPolicyRegistry {
    constructor(policies = []) {
      this.policies = new Map();
      policies.forEach((policy) => this.register(policy));
    }

    register(policyDefinition) {
      const policy = normalizeOperationPolicy(policyDefinition);
      if (this.policies.has(policy.id)) {
        throw new OperationPolicyError("DUPLICATE_OPERATION_POLICY", `Operation policy already exists: ${policy.id}`);
      }
      this.policies.set(policy.id, policy);
      return this;
    }

    replace(policyDefinition) {
      const policy = normalizeOperationPolicy(policyDefinition);
      this.policies.set(policy.id, policy);
      return this;
    }

    resolve(operationId) {
      const id = String(operationId || "").trim().toLowerCase();
      return this.policies.get(id) || Object.freeze({
        version: FAILURE_POLICY_VERSION,
        id,
        safetyClass: "restricted",
        retryMode: "never",
        queueMode: "none",
        mayReplay: false,
        requiresExpiry: false,
        requiresDurableAudit: true,
        ambiguousDeliveryIsUnknown: true,
        description: "Unknown operations are restricted until explicitly classified."
      });
    }

    list() {
      return [...this.policies.values()];
    }

    evaluate(operationId, context = {}) {
      const policy = this.resolve(operationId);
      const nowMs = toTime(context.now, Date.now());
      const expiresAtMs = toTime(context.expiresAt, NaN);

      if (policy.requiresExpiry && !Number.isFinite(expiresAtMs)) {
        return decision(policy, "blocked", "COMMAND_EXPIRY_REQUIRED");
      }
      if (policy.requiresExpiry && expiresAtMs <= nowMs) {
        return decision(policy, "expired", "COMMAND_EXPIRED");
      }
      if (context.authorizationAvailable === false) {
        return decision(policy, "blocked", "AUTHORIZATION_UNAVAILABLE");
      }
      if (policy.requiresDurableAudit && context.durableAuditAvailable !== true) {
        const auditFailureMode = context.auditFailureMode || "secure-default";
        if (auditFailureMode !== "allow-with-critical-warning") {
          return decision(policy, "blocked", "DURABLE_AUDIT_REQUIRED");
        }
      }
      return decision(policy, "allowed", "");
    }
  }

  function toTime(value, fallback) {
    if (value instanceof Date) return value.getTime();
    if (typeof value === "number") return Number.isFinite(value) ? value : fallback;
    const parsed = Date.parse(value);
    return Number.isFinite(parsed) ? parsed : fallback;
  }

  function decision(policy, outcome, reasonCode) {
    return Object.freeze({
      operationId: policy.id,
      outcome,
      allowed: outcome === "allowed",
      reasonCode,
      policy
    });
  }

  function assessDeliveryOutcome(operationId, {
    transmitted = false,
    confirmed = false,
    registry = createDefaultOperationPolicyRegistry()
  } = {}) {
    const policy = registry.resolve(operationId);
    if (transmitted && !confirmed && policy.ambiguousDeliveryIsUnknown) {
      return Object.freeze({
        operationId: policy.id,
        outcome: "outcome-unknown",
        reasonCode: "TRANSMITTED_WITHOUT_CONFIRMATION",
        mayReplay: false
      });
    }
    return Object.freeze({
      operationId: policy.id,
      outcome: confirmed ? "confirmed" : "not-transmitted",
      reasonCode: "",
      mayReplay: policy.mayReplay
    });
  }

  const DEFAULT_OPERATION_POLICIES = Object.freeze([
    { id: OPERATION_IDS.INVENTORY_READ, safetyClass: "safe-read", retryMode: "bounded-safe" },
    {
      id: OPERATION_IDS.INVENTORY_WRITE,
      safetyClass: "versioned-mutation",
      retryMode: "idempotency-required",
      requiresDurableAudit: true
    },
    { id: OPERATION_IDS.DEVICE_DISCOVERY, safetyClass: "safe-read", retryMode: "bounded-safe" },
    { id: OPERATION_IDS.DEVICE_HEALTH, safetyClass: "safe-read", retryMode: "bounded-safe" },
    {
      id: OPERATION_IDS.DEVICE_EVENTS,
      safetyClass: "event-ingest",
      retryMode: "idempotency-required",
      queueMode: "durable-outbox",
      mayReplay: true
    },
    { id: OPERATION_IDS.LIVE_VIEW, safetyClass: "safe-read", retryMode: "bounded-safe" },
    { id: OPERATION_IDS.PLAYBACK, safetyClass: "safe-read", retryMode: "bounded-safe" },
    { id: OPERATION_IDS.DOWNLOAD, safetyClass: "transfer", retryMode: "capability-gated" },
    {
      id: OPERATION_IDS.ALARM_ACKNOWLEDGE,
      safetyClass: "versioned-mutation",
      retryMode: "idempotency-required",
      queueMode: "durable-outbox",
      mayReplay: true,
      requiresDurableAudit: true
    },
    {
      id: OPERATION_IDS.AUDIT_APPEND,
      safetyClass: "event-ingest",
      retryMode: "idempotency-required",
      queueMode: "durable-outbox",
      mayReplay: true
    },
    {
      id: OPERATION_IDS.PTZ,
      safetyClass: "ephemeral-physical",
      retryMode: "never",
      requiresDurableAudit: true
    },
    {
      id: OPERATION_IDS.ACCESS_UNLOCK,
      safetyClass: "ephemeral-physical",
      retryMode: "never",
      requiresDurableAudit: true
    },
    {
      id: OPERATION_IDS.BARRIER_OPEN,
      safetyClass: "ephemeral-physical",
      retryMode: "never",
      requiresDurableAudit: true
    },
    {
      id: OPERATION_IDS.RELAY_ACTIVATE,
      safetyClass: "ephemeral-physical",
      retryMode: "never",
      requiresDurableAudit: true
    },
    {
      id: OPERATION_IDS.BROADCAST,
      safetyClass: "ephemeral-physical",
      retryMode: "never",
      requiresDurableAudit: true
    },
    {
      id: OPERATION_IDS.REBOOT,
      safetyClass: "destructive",
      retryMode: "operator-confirmation",
      requiresDurableAudit: true
    },
    {
      id: OPERATION_IDS.SHUTDOWN,
      safetyClass: "destructive",
      retryMode: "operator-confirmation",
      requiresDurableAudit: true
    },
    {
      id: OPERATION_IDS.FORMAT_STORAGE,
      safetyClass: "destructive",
      retryMode: "never",
      requiresDurableAudit: true
    },
    {
      id: OPERATION_IDS.DELETE_RECORDING,
      safetyClass: "destructive",
      retryMode: "never",
      requiresDurableAudit: true
    },
    {
      id: OPERATION_IDS.CHANGE_PASSWORD,
      safetyClass: "destructive",
      retryMode: "operator-confirmation",
      requiresDurableAudit: true
    },
    {
      id: OPERATION_IDS.UPDATE_FIRMWARE,
      safetyClass: "destructive",
      retryMode: "operator-confirmation",
      requiresDurableAudit: true
    }
  ]);

  function createDefaultOperationPolicyRegistry() {
    return new OperationPolicyRegistry(DEFAULT_OPERATION_POLICIES);
  }

  return Object.freeze({
    DEFAULT_OPERATION_POLICIES,
    OPERATION_IDS,
    OPERATION_OUTCOMES,
    OperationPolicyError,
    OperationPolicyRegistry,
    assessDeliveryOutcome,
    createDefaultOperationPolicyRegistry,
    normalizeOperationPolicy
  });
});
