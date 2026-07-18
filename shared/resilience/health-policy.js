(function healthPolicyModule(root, factory) {
  const contract = typeof module === "object" && module.exports
    ? require("./contract")
    : root.VmsResilienceContract;
  const api = factory(contract);
  if (typeof module === "object" && module.exports) {
    module.exports = api;
  } else {
    root.VmsResilienceHealth = api;
  }
})(typeof globalThis !== "undefined" ? globalThis : this, function createHealthPolicy(contract) {
  "use strict";

  const {
    ADAPTER_STATES,
    FRESHNESS_STATES,
    FUNCTIONAL_HEALTH_STATES,
    REACHABILITY_STATES,
    RECORDING_STATES,
    STORAGE_STATES,
    enumValue
  } = contract;

  function createHealthSnapshot(input = {}) {
    return Object.freeze({
      resourceId: String(input.resourceId || ""),
      parentId: String(input.parentId || ""),
      reachability: enumValue(input.reachability, REACHABILITY_STATES, "unknown"),
      freshness: enumValue(input.freshness, FRESHNESS_STATES, "unknown"),
      functional: enumValue(input.functional, FUNCTIONAL_HEALTH_STATES, "unknown"),
      recording: enumValue(input.recording, RECORDING_STATES, "unknown"),
      storage: enumValue(input.storage, STORAGE_STATES, "unknown"),
      adapter: enumValue(input.adapter, ADAPTER_STATES, "unknown"),
      reasonCode: String(input.reasonCode || ""),
      message: String(input.message || ""),
      lastObservedAt: input.lastObservedAt || null,
      lastTransitionAt: input.lastTransitionAt || null,
      lastKnown: input.lastKnown ? Object.freeze({ ...input.lastKnown }) : null
    });
  }

  function fromLegacyStatus(status, input = {}) {
    const normalized = String(status || "").trim().toLowerCase();
    const mapping = {
      online: { reachability: "reachable", freshness: "current", functional: "healthy" },
      warning: { reachability: "reachable", freshness: "current", functional: "degraded" },
      offline: { reachability: "unreachable", freshness: "current", functional: "unknown" },
      unknown: { reachability: "unknown", freshness: "unknown", functional: "unknown" }
    };
    return createHealthSnapshot({ ...input, ...(mapping[normalized] || mapping.unknown) });
  }

  function deriveDependentSnapshot(parentInput, childInput = {}) {
    const parent = createHealthSnapshot(parentInput);
    const child = createHealthSnapshot(childInput);
    if (parent.reachability !== "unreachable" && parent.reachability !== "unknown") {
      return child;
    }

    return createHealthSnapshot({
      ...child,
      reachability: "unknown",
      freshness: "stale",
      functional: "unknown",
      recording: child.recording === "not-applicable" ? "not-applicable" : "unknown",
      storage: child.storage === "not-applicable" ? "not-applicable" : "unknown",
      reasonCode: parent.reachability === "unreachable" ? "PARENT_UNREACHABLE" : "PARENT_STATUS_UNKNOWN",
      message: `Current status cannot be verified because ${parent.resourceId || "the parent resource"} is unavailable.`,
      lastKnown: {
        reachability: child.reachability,
        functional: child.functional,
        recording: child.recording,
        storage: child.storage,
        lastObservedAt: child.lastObservedAt
      }
    });
  }

  function summarizeHealth(input = {}) {
    const health = createHealthSnapshot(input);
    if (health.functional === "maintenance") return summary("maintenance", "Maintenance", health);
    if (health.functional === "recovering") return summary("recovering", "Recovering", health);
    if (health.freshness === "stale") {
      const lastKnown = health.lastKnown?.reachability === "reachable" ? "Last known reachable" : "Last known state";
      return summary("unknown", `${lastKnown} — current status unknown`, health);
    }
    if (health.reachability === "unreachable") return summary("unreachable", "Unreachable", health);
    if (health.reachability === "unknown") return summary("unknown", "Unknown", health);
    if (
      health.reachability === "degraded" ||
      health.functional === "degraded" ||
      health.functional === "failed" ||
      health.storage === "degraded" ||
      health.storage === "failed" ||
      health.adapter === "degraded"
    ) {
      return summary("degraded", "Degraded", health);
    }
    if (
      health.reachability === "reachable" &&
      health.freshness === "current" &&
      health.functional === "healthy"
    ) {
      return summary("online", "Online", health);
    }
    return summary("unknown", "Unknown", health);
  }

  function summary(state, label, dimensions) {
    return Object.freeze({ state, label, dimensions });
  }

  function createRootCause({
    id,
    resourceId,
    reasonCode,
    affectedResourceIds = [],
    observedAt = null,
    message = ""
  } = {}) {
    const affected = [...new Set(affectedResourceIds.map(String).filter(Boolean))].sort();
    return Object.freeze({
      id: String(id || ""),
      resourceId: String(resourceId || ""),
      reasonCode: String(reasonCode || "UNKNOWN_ROOT_CAUSE"),
      affectedResourceIds: Object.freeze(affected),
      observedAt,
      message: String(message || ""),
      alarmCount: 1
    });
  }

  function applyParentFailureToTree(resources = [], parentId) {
    const parent = resources.find((resource) => resource.resourceId === parentId);
    if (!parent) return resources.map(createHealthSnapshot);
    return resources.map((resource) => {
      if (resource.parentId !== parentId) return createHealthSnapshot(resource);
      return deriveDependentSnapshot(parent, resource);
    });
  }

  function confirmedNotRecording(input = {}) {
    const health = createHealthSnapshot(input);
    return health.recording === "not-recording" &&
      health.freshness === "current" &&
      health.reasonCode !== "PARENT_UNREACHABLE" &&
      health.reasonCode !== "PARENT_STATUS_UNKNOWN";
  }

  return Object.freeze({
    applyParentFailureToTree,
    confirmedNotRecording,
    createHealthSnapshot,
    createRootCause,
    deriveDependentSnapshot,
    fromLegacyStatus,
    summarizeHealth
  });
});
