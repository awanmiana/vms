const resilience = require("../../shared/resilience");

class ResiliencePolicyService {
  constructor({
    availabilityPolicies = resilience.createDefaultAvailabilityPolicyRegistry(),
    operationPolicies = resilience.createDefaultOperationPolicyRegistry(),
    resourcePolicy = new resilience.ResourcePolicy(),
    auditFailureMode = "secure-default"
  } = {}) {
    requireMethod(availabilityPolicies, "evaluate", "availabilityPolicies");
    requireMethod(operationPolicies, "evaluate", "operationPolicies");
    requireMethod(operationPolicies, "resolve", "operationPolicies");
    requireMethod(resourcePolicy, "sort", "resourcePolicy");

    this.availabilityPolicies = availabilityPolicies;
    this.operationPolicies = operationPolicies;
    this.resourcePolicy = resourcePolicy;
    this.auditFailureMode = String(auditFailureMode || "secure-default");
  }

  functionAvailability(functionId, dependencies = {}) {
    return this.availabilityPolicies.evaluate(functionId, dependencies);
  }

  operationPreflight(operationId, context = {}) {
    return this.operationPolicies.evaluate(operationId, {
      auditFailureMode: this.auditFailureMode,
      ...context
    });
  }

  operationPolicy(operationId) {
    return this.operationPolicies.resolve(operationId);
  }

  deliveryOutcome(operationId, context = {}) {
    return resilience.assessDeliveryOutcome(operationId, {
      ...context,
      registry: this.operationPolicies
    });
  }

  retryDecision(input = {}) {
    return resilience.retryDecision({
      ...input,
      registry: this.operationPolicies
    });
  }

  healthSnapshot(input = {}) {
    return resilience.createHealthSnapshot(input);
  }

  dependentHealth(parent, child) {
    return resilience.deriveDependentSnapshot(parent, child);
  }

  summarizeHealth(input = {}) {
    return resilience.summarizeHealth(input);
  }

  prioritizeWork(items = []) {
    return this.resourcePolicy.sort(items);
  }

  degradationPlan(stage) {
    return resilience.degradationPlan(stage);
  }

  recoveryPlan(items = []) {
    return resilience.sortRecoveryItems(items);
  }
}

function requireMethod(value, method, label) {
  if (!value || typeof value[method] !== "function") {
    throw new TypeError(`${label} must provide ${method}().`);
  }
}

module.exports = {
  ResiliencePolicyService
};
