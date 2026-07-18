(function availabilityPolicyModule(root, factory) {
  const contract = typeof module === "object" && module.exports
    ? require("./contract")
    : root.VmsResilienceContract;
  const api = factory(contract);
  if (typeof module === "object" && module.exports) {
    module.exports = api;
  } else {
    root.VmsResilienceAvailability = api;
  }
})(typeof globalThis !== "undefined" ? globalThis : this, function createAvailabilityPolicy(contract) {
  "use strict";

  const {
    AVAILABILITY_STATES,
    DEPENDENCY_IDS: D,
    FAILURE_POLICY_VERSION,
    FUNCTION_IDS: F,
    normalizeDependencySnapshot
  } = contract;

  class AvailabilityPolicyError extends Error {
    constructor(code, message) {
      super(message);
      this.name = "AvailabilityPolicyError";
      this.code = code;
    }
  }

  function freezePolicy(policy) {
    if (!policy || typeof policy !== "object" || Array.isArray(policy)) {
      throw new AvailabilityPolicyError("INVALID_AVAILABILITY_POLICY", "Availability policy must be an object.");
    }
    const id = String(policy.id || "").trim();
    if (!/^[a-z][a-z0-9-]*(?:\.[a-z][a-z0-9-]*)+$/.test(id)) {
      throw new AvailabilityPolicyError("INVALID_FUNCTION_ID", `Invalid function id: ${policy.id || "(empty)"}`);
    }
    const requiredAll = Object.freeze([...(policy.requiredAll || [])]);
    const requiredAny = Object.freeze([...(policy.requiredAny || [])]);
    return Object.freeze({
      version: FAILURE_POLICY_VERSION,
      id,
      description: String(policy.description || "").trim(),
      requiredAll,
      requiredAny,
      externalIndependent: Boolean(policy.externalIndependent),
      cacheFallbackDependency: String(policy.cacheFallbackDependency || "").trim(),
      cacheFallbackMode: policy.cacheFallbackMode || "read-only",
      unavailableReasonCode: policy.unavailableReasonCode || "REQUIRED_DEPENDENCY_UNAVAILABLE"
    });
  }

  class AvailabilityPolicyRegistry {
    constructor(policies = []) {
      this.policies = new Map();
      policies.forEach((policy) => this.register(policy));
    }

    register(policyDefinition) {
      const policy = freezePolicy(policyDefinition);
      if (this.policies.has(policy.id)) {
        throw new AvailabilityPolicyError("DUPLICATE_FUNCTION_POLICY", `Function policy already exists: ${policy.id}`);
      }
      this.policies.set(policy.id, policy);
      return this;
    }

    replace(policyDefinition) {
      const policy = freezePolicy(policyDefinition);
      this.policies.set(policy.id, policy);
      return this;
    }

    resolve(functionId) {
      return this.policies.get(String(functionId || "").trim()) || null;
    }

    list() {
      return [...this.policies.values()];
    }

    evaluate(functionId, dependencyInput = {}) {
      const policy = this.resolve(functionId);
      const dependencies = normalizeDependencySnapshot(dependencyInput);
      if (!policy) {
        return Object.freeze({
          functionId: String(functionId || ""),
          mode: "unknown",
          allowed: false,
          stale: true,
          sourceOperationUnaffected: false,
          reasonCode: "FUNCTION_POLICY_NOT_REGISTERED",
          dependencies
        });
      }

      if (policy.externalIndependent) {
        return Object.freeze({
          functionId: policy.id,
          mode: "independent",
          allowed: true,
          stale: false,
          sourceOperationUnaffected: true,
          reasonCode: "VMS_NOT_IN_SOURCE_OPERATING_PATH",
          dependencies
        });
      }

      const allStates = policy.requiredAll.map((id) => ({ id, state: dependencies[id] || "unknown" }));
      const anyStates = policy.requiredAny.map((id) => ({ id, state: dependencies[id] || "unknown" }));
      const allSatisfied = allStates.every(({ state }) => state === "available" || state === "degraded");
      const anySatisfied = anyStates.length === 0 ||
        anyStates.some(({ state }) => state === "available" || state === "degraded");

      if (!allSatisfied || !anySatisfied) {
        const cacheState = dependencies[policy.cacheFallbackDependency] || "unknown";
        if (policy.cacheFallbackDependency && (cacheState === "available" || cacheState === "degraded")) {
          return Object.freeze({
            functionId: policy.id,
            mode: policy.cacheFallbackMode,
            allowed: true,
            stale: true,
            sourceOperationUnaffected: false,
            reasonCode: "LAST_KNOWN_GOOD_CACHE",
            dependencies
          });
        }

        const relevantStates = [
          ...allStates.filter(({ state }) => state !== "available" && state !== "degraded"),
          ...(anySatisfied ? [] : anyStates)
        ];
        const explicitlyUnavailable = relevantStates.some(({ state }) => state === "unavailable");
        return Object.freeze({
          functionId: policy.id,
          mode: explicitlyUnavailable ? "unavailable" : "unknown",
          allowed: false,
          stale: true,
          sourceOperationUnaffected: false,
          reasonCode: policy.unavailableReasonCode,
          dependencies,
          blockingDependencies: Object.freeze(relevantStates)
        });
      }

      const degraded = [...allStates, ...anyStates].some(({ state }) => state === "degraded");
      return Object.freeze({
        functionId: policy.id,
        mode: degraded ? "degraded" : "available",
        allowed: true,
        stale: false,
        sourceOperationUnaffected: false,
        reasonCode: degraded ? "DEPENDENCY_DEGRADED" : "",
        dependencies
      });
    }
  }

  const DEFAULT_POLICIES = Object.freeze([
    {
      id: F.NATIVE_DEVICE_OPERATION,
      externalIndependent: true,
      description: "A camera, recorder, or controller operates independently of the VMS."
    },
    {
      id: F.NATIVE_RECORDING,
      externalIndependent: true,
      description: "Recorder-owned recording is not performed by the VMS."
    },
    {
      id: F.INVENTORY_READ,
      requiredAny: [D.INVENTORY_AUTHORITY],
      cacheFallbackDependency: D.INVENTORY_CACHE,
      cacheFallbackMode: "read-only"
    },
    {
      id: F.INVENTORY_WRITE,
      requiredAll: [D.INVENTORY_AUTHORITY, D.AUTHORIZATION],
      unavailableReasonCode: "INVENTORY_AUTHORITY_REQUIRED"
    },
    {
      id: F.MONITORING_COLLECT,
      requiredAll: [D.CONTINUITY_PROVIDER, D.SOURCE_DEVICE],
      unavailableReasonCode: "CONTINUITY_PROVIDER_OR_SOURCE_UNAVAILABLE"
    },
    {
      id: F.LOCAL_DEVICE_READ,
      requiredAll: [D.LOCAL_NETWORK, D.SOURCE_DEVICE, D.AUTHORIZATION]
    },
    {
      id: F.LIVE_VIEW,
      requiredAll: [D.LOCAL_NETWORK, D.SOURCE_DEVICE, D.AUTHORIZATION]
    },
    {
      id: F.PLAYBACK,
      requiredAll: [D.LOCAL_NETWORK, D.RECORDING_SOURCE, D.AUTHORIZATION]
    },
    {
      id: F.DOWNLOAD,
      requiredAll: [D.LOCAL_NETWORK, D.RECORDING_SOURCE, D.AUTHORIZATION]
    },
    {
      id: F.SHARED_CONFIGURATION_WRITE,
      requiredAll: [D.COORDINATOR, D.INVENTORY_AUTHORITY, D.AUTHORIZATION, D.DURABLE_AUDIT],
      unavailableReasonCode: "SHARED_CONFIGURATION_AUTHORITY_REQUIRED"
    },
    {
      id: F.ALARM_COLLECT,
      requiredAll: [D.CONTINUITY_PROVIDER, D.SOURCE_DEVICE, D.DURABLE_ALARM]
    },
    {
      id: F.ALARM_SYNC,
      requiredAll: [D.WAN, D.COORDINATOR, D.DURABLE_ALARM]
    },
    {
      id: F.AUDIT_SYNC,
      requiredAll: [D.WAN, D.COORDINATOR, D.DURABLE_AUDIT]
    },
    {
      id: F.REMOTE_CROSS_SITE,
      requiredAll: [D.WAN, D.COORDINATOR, D.AUTHORIZATION]
    },
    {
      id: F.VENDOR_SERVICE_OPERATION,
      requiredAll: [D.VENDOR_SERVICE],
      unavailableReasonCode: "OPTIONAL_VENDOR_SERVICE_UNAVAILABLE"
    },
    {
      id: F.LICENSED_COMPONENT_OPERATION,
      requiredAll: [D.LICENSED_COMPONENT],
      unavailableReasonCode: "OPTIONAL_LICENSED_COMPONENT_UNAVAILABLE"
    }
  ]);

  function createDefaultAvailabilityPolicyRegistry() {
    return new AvailabilityPolicyRegistry(DEFAULT_POLICIES);
  }

  function availabilityState(value) {
    return AVAILABILITY_STATES.includes(value) ? value : "unknown";
  }

  return Object.freeze({
    AvailabilityPolicyError,
    AvailabilityPolicyRegistry,
    DEFAULT_POLICIES,
    availabilityState,
    createDefaultAvailabilityPolicyRegistry
  });
});
