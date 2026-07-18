(function recoveryPolicyModule(root, factory) {
  const contract = typeof module === "object" && module.exports
    ? require("./contract")
    : root.VmsResilienceContract;
  const operations = typeof module === "object" && module.exports
    ? require("./operation-policy")
    : root.VmsResilienceOperations;
  const api = factory(contract, operations);
  if (typeof module === "object" && module.exports) {
    module.exports = api;
  } else {
    root.VmsResilienceRecovery = api;
  }
})(typeof globalThis !== "undefined" ? globalThis : this, function createRecoveryPolicy(contract, operations) {
  "use strict";

  const { ERROR_CATEGORIES, RECOVERY_PHASES } = contract;

  const TRANSIENT_ERRORS = new Set([
    "network-temporary",
    "timeout",
    "throttled",
    "server-temporary"
  ]);

  const NON_RETRYABLE_ERRORS = new Set([
    "authentication",
    "authorization",
    "validation",
    "unsupported",
    "unverified",
    "permanent"
  ]);

  function calculateFullJitterDelay({
    attempt,
    baseMs,
    capMs,
    random = Math.random
  } = {}) {
    if (!Number.isInteger(attempt) || attempt < 0) {
      throw new RangeError("attempt must be a non-negative integer.");
    }
    if (!Number.isFinite(baseMs) || baseMs <= 0) {
      throw new RangeError("baseMs must be a positive number.");
    }
    if (!Number.isFinite(capMs) || capMs < baseMs) {
      throw new RangeError("capMs must be greater than or equal to baseMs.");
    }
    const randomValue = Number(random());
    if (!Number.isFinite(randomValue) || randomValue < 0 || randomValue > 1) {
      throw new RangeError("random must return a number from 0 through 1.");
    }
    const maximum = Math.min(capMs, baseMs * (2 ** attempt));
    return Math.floor(randomValue * maximum);
  }

  function retryDecision({
    operationId,
    errorCategory,
    idempotencyKey = "",
    expectedVersion,
    resumeCapability = false,
    stableSourceValidator = false,
    registry = operations.createDefaultOperationPolicyRegistry()
  } = {}) {
    const policy = registry.resolve(operationId);
    const category = ERROR_CATEGORIES.includes(errorCategory) ? errorCategory : "unknown";

    if (NON_RETRYABLE_ERRORS.has(category)) {
      return result(false, "ERROR_IS_NOT_RETRYABLE", policy, category);
    }
    if (!TRANSIENT_ERRORS.has(category)) {
      return result(false, "ERROR_RETRYABILITY_UNKNOWN", policy, category);
    }

    switch (policy.retryMode) {
      case "bounded-safe":
        return result(true, "BOUNDED_SAFE_RETRY", policy, category);
      case "idempotency-required":
        if (!String(idempotencyKey || "").trim() || expectedVersion === undefined || expectedVersion === null) {
          return result(false, "IDEMPOTENCY_AND_PRECONDITION_REQUIRED", policy, category);
        }
        return result(true, "IDEMPOTENT_RETRY", policy, category);
      case "capability-gated":
        if (!resumeCapability || !stableSourceValidator) {
          return result(false, "VERIFIED_RESUME_CAPABILITY_REQUIRED", policy, category);
        }
        return result(true, "VERIFIED_TRANSFER_RESUME", policy, category);
      case "operator-confirmation":
        return result(false, "NEW_OPERATOR_CONFIRMATION_REQUIRED", policy, category);
      default:
        return result(false, "OPERATION_MUST_NOT_RETRY", policy, category);
    }
  }

  function result(retry, reasonCode, policy, errorCategory) {
    return Object.freeze({
      retry,
      reasonCode,
      operationId: policy.id,
      retryMode: policy.retryMode,
      errorCategory
    });
  }

  function sortRecoveryItems(items = []) {
    const phaseRank = new Map(RECOVERY_PHASES.map((phase, index) => [phase, index]));
    return items
      .map((item, index) => ({ ...item, __index: index }))
      .sort((left, right) => {
        const phaseDifference = (phaseRank.get(left.phase) ?? Number.MAX_SAFE_INTEGER) -
          (phaseRank.get(right.phase) ?? Number.MAX_SAFE_INTEGER);
        if (phaseDifference !== 0) return phaseDifference;
        const leftTime = Date.parse(left.requestedAt || "") || 0;
        const rightTime = Date.parse(right.requestedAt || "") || 0;
        if (leftTime !== rightTime) return leftTime - rightTime;
        return left.__index - right.__index;
      })
      .map(({ __index, ...item }) => item);
  }

  function recoveryBudgetKey({ siteId = "standalone", dependencyId = "default" } = {}) {
    return `${String(siteId || "standalone")}::${String(dependencyId || "default")}`;
  }

  function downloadResumeDecision({
    partialBytes = 0,
    rangeSupported = false,
    stableSourceValidator = false,
    sourceDigestAvailable = false
  } = {}) {
    if (!Number.isFinite(partialBytes) || partialBytes <= 0) {
      return Object.freeze({ action: "start", sourceEqualityVerifiable: Boolean(sourceDigestAvailable) });
    }
    if (rangeSupported && stableSourceValidator) {
      return Object.freeze({
        action: "resume",
        sourceEqualityVerifiable: Boolean(sourceDigestAvailable),
        reasonCode: "STABLE_SOURCE_RANGE_RESUME"
      });
    }
    return Object.freeze({
      action: "restart",
      sourceEqualityVerifiable: false,
      reasonCode: "SOURCE_IDENTITY_NOT_VERIFIABLE"
    });
  }

  return Object.freeze({
    NON_RETRYABLE_ERRORS,
    TRANSIENT_ERRORS,
    calculateFullJitterDelay,
    downloadResumeDecision,
    recoveryBudgetKey,
    retryDecision,
    sortRecoveryItems
  });
});
