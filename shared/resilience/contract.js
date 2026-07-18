(function resilienceContractModule(root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) {
    module.exports = api;
  } else {
    root.VmsResilienceContract = api;
  }
})(typeof globalThis !== "undefined" ? globalThis : this, function createResilienceContract() {
  "use strict";

  const FAILURE_POLICY_VERSION = 1;

  const AVAILABILITY_STATES = Object.freeze([
    "available",
    "degraded",
    "unavailable",
    "unknown"
  ]);

  const AVAILABILITY_MODES = Object.freeze([
    "available",
    "degraded",
    "read-only",
    "unavailable",
    "unknown",
    "independent"
  ]);

  const DEPENDENCY_IDS = Object.freeze({
    UI_RUNTIME: "ui-runtime",
    LOCAL_RUNTIME: "local-runtime",
    CONTINUITY_PROVIDER: "continuity-provider",
    LOCAL_NETWORK: "local-network",
    WAN: "wan",
    COORDINATOR: "coordinator",
    INVENTORY_AUTHORITY: "inventory-authority",
    INVENTORY_CACHE: "inventory-cache",
    SOURCE_DEVICE: "source-device",
    RECORDING_SOURCE: "recording-source",
    AUTHORIZATION: "authorization",
    DURABLE_ALARM: "durable-alarm",
    DURABLE_AUDIT: "durable-audit",
    VENDOR_SERVICE: "vendor-service",
    LICENSED_COMPONENT: "licensed-component"
  });

  const FUNCTION_IDS = Object.freeze({
    NATIVE_DEVICE_OPERATION: "native.device-operation",
    NATIVE_RECORDING: "native.recording",
    INVENTORY_READ: "inventory.read",
    INVENTORY_WRITE: "inventory.write",
    MONITORING_COLLECT: "monitoring.collect",
    LOCAL_DEVICE_READ: "device.local-read",
    LIVE_VIEW: "media.live-view",
    PLAYBACK: "media.playback",
    DOWNLOAD: "media.download",
    SHARED_CONFIGURATION_WRITE: "configuration.shared-write",
    ALARM_COLLECT: "alarm.collect",
    ALARM_SYNC: "alarm.sync",
    AUDIT_SYNC: "audit.sync",
    REMOTE_CROSS_SITE: "remote.cross-site",
    VENDOR_SERVICE_OPERATION: "vendor-service.operation",
    LICENSED_COMPONENT_OPERATION: "licensed-component.operation"
  });

  const OPERATION_SAFETY_CLASSES = Object.freeze([
    "safe-read",
    "versioned-mutation",
    "event-ingest",
    "transfer",
    "ephemeral-physical",
    "destructive",
    "restricted"
  ]);

  const RETRY_MODES = Object.freeze([
    "bounded-safe",
    "idempotency-required",
    "capability-gated",
    "operator-confirmation",
    "never"
  ]);

  const QUEUE_MODES = Object.freeze([
    "none",
    "durable-outbox"
  ]);

  const ERROR_CATEGORIES = Object.freeze([
    "network-temporary",
    "timeout",
    "throttled",
    "server-temporary",
    "authentication",
    "authorization",
    "validation",
    "unsupported",
    "unverified",
    "permanent",
    "unknown"
  ]);

  const REACHABILITY_STATES = Object.freeze([
    "reachable",
    "degraded",
    "unreachable",
    "unknown"
  ]);

  const FRESHNESS_STATES = Object.freeze([
    "current",
    "stale",
    "unknown"
  ]);

  const FUNCTIONAL_HEALTH_STATES = Object.freeze([
    "healthy",
    "degraded",
    "failed",
    "unknown",
    "maintenance",
    "recovering"
  ]);

  const RECORDING_STATES = Object.freeze([
    "recording",
    "not-recording",
    "unknown",
    "not-applicable"
  ]);

  const STORAGE_STATES = Object.freeze([
    "healthy",
    "degraded",
    "failed",
    "unknown",
    "not-applicable"
  ]);

  const ADAPTER_STATES = Object.freeze([
    "available",
    "degraded",
    "unsupported",
    "unverified",
    "unavailable",
    "unknown"
  ]);

  const DEVICE_ACTIVITY_PRIORITIES = Object.freeze([
    "high",
    "medium",
    "low",
    "idle"
  ]);

  const WORK_PRIORITIES = Object.freeze([
    "system-critical",
    ...DEVICE_ACTIVITY_PRIORITIES
  ]);

  const RECOVERY_PHASES = Object.freeze([
    "control-and-authorization",
    "alarm-and-audit-sync",
    "device-health",
    "selected-live-media",
    "playback-and-download",
    "background-low",
    "background-idle"
  ]);

  function enumValue(value, allowed, fallback) {
    return allowed.includes(value) ? value : fallback;
  }

  function normalizeDependencyState(value) {
    return enumValue(value, AVAILABILITY_STATES, "unknown");
  }

  function normalizeDependencySnapshot(snapshot = {}) {
    return Object.freeze(Object.fromEntries(
      Object.entries(snapshot).map(([dependencyId, state]) => [
        dependencyId,
        normalizeDependencyState(state)
      ])
    ));
  }

  return Object.freeze({
    FAILURE_POLICY_VERSION,
    ADAPTER_STATES,
    AVAILABILITY_MODES,
    AVAILABILITY_STATES,
    DEPENDENCY_IDS,
    DEVICE_ACTIVITY_PRIORITIES,
    ERROR_CATEGORIES,
    FRESHNESS_STATES,
    FUNCTION_IDS,
    FUNCTIONAL_HEALTH_STATES,
    OPERATION_SAFETY_CLASSES,
    QUEUE_MODES,
    REACHABILITY_STATES,
    RECORDING_STATES,
    RECOVERY_PHASES,
    RETRY_MODES,
    STORAGE_STATES,
    WORK_PRIORITIES,
    enumValue,
    normalizeDependencySnapshot,
    normalizeDependencyState
  });
});
