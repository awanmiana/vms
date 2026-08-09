(function outcomeMappingModule(root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.VmsResilienceOutcomes = api;
})(typeof globalThis !== "undefined" ? globalThis : this, function createOutcomeMapping() {
  "use strict";

  const OPERATION_OUTCOMES = Object.freeze([
    "allowed",
    "succeeded",
    "failed",
    "blocked",
    "expired",
    "unsupported",
    "unknown",
    "unavailable",
    "outcome-unknown"
  ]);

  const ADAPTER_STATUS_OUTCOMES = Object.freeze({
    succeeded: "succeeded",
    failed: "failed",
    unsupported: "unsupported",
    unknown: "unknown",
    unavailable: "unavailable"
  });

  const POLICY_OUTCOMES = Object.freeze({
    allowed: "allowed",
    blocked: "blocked",
    expired: "expired",
    "outcome-unknown": "outcome-unknown",
    confirmed: "succeeded",
    "not-transmitted": "failed"
  });

  const COMMAND_STATUS_OUTCOMES = Object.freeze({
    executed: "succeeded",
    rejected: "failed",
    needs_confirmation: "blocked"
  });

  const PERSISTENCE_CODE_OUTCOMES = Object.freeze({
    PERSISTENCE_UNAVAILABLE: "unavailable",
    PERSISTENCE_SCHEMA_INVALID: "failed"
  });

  const HTTP_STATUS_BY_OUTCOME = Object.freeze({
    allowed: 200,
    succeeded: 200,
    failed: 500,
    blocked: 403,
    expired: 409,
    unsupported: 501,
    unknown: 503,
    unavailable: 503,
    "outcome-unknown": 502
  });

  function adapterOutcome(result = {}) {
    if (result.outcomeUnknown === true) return "outcome-unknown";
    return ADAPTER_STATUS_OUTCOMES[result.status] || "unknown";
  }

  function policyOutcome(result = {}) {
    return POLICY_OUTCOMES[result.outcome] || "unknown";
  }

  function commandOutcome(result = {}) {
    return COMMAND_STATUS_OUTCOMES[result.status] || "unknown";
  }

  function persistenceOutcome(error = {}) {
    return PERSISTENCE_CODE_OUTCOMES[error.code] || "failed";
  }

  function httpStatusForOutcome(outcome) {
    return HTTP_STATUS_BY_OUTCOME[outcome] || 500;
  }

  return Object.freeze({
    ADAPTER_STATUS_OUTCOMES,
    COMMAND_STATUS_OUTCOMES,
    HTTP_STATUS_BY_OUTCOME,
    OPERATION_OUTCOMES,
    PERSISTENCE_CODE_OUTCOMES,
    POLICY_OUTCOMES,
    adapterOutcome,
    commandOutcome,
    httpStatusForOutcome,
    persistenceOutcome,
    policyOutcome
  });
});
