(function resilienceIndexModule(root, factory) {
  const modules = typeof module === "object" && module.exports
    ? {
        contract: require("./contract"),
        availability: require("./availability-policy"),
        operations: require("./operation-policy"),
        health: require("./health-policy"),
        recovery: require("./recovery-policy"),
        resources: require("./resource-policy")
      }
    : {
        contract: root.VmsResilienceContract,
        availability: root.VmsResilienceAvailability,
        operations: root.VmsResilienceOperations,
        health: root.VmsResilienceHealth,
        recovery: root.VmsResilienceRecovery,
        resources: root.VmsResilienceResources
      };
  const api = factory(modules);
  if (typeof module === "object" && module.exports) {
    module.exports = api;
  } else {
    root.VmsResilience = api;
  }
})(typeof globalThis !== "undefined" ? globalThis : this, function createResilienceIndex(modules) {
  "use strict";

  return Object.freeze({
    ...modules.contract,
    ...modules.availability,
    ...modules.operations,
    ...modules.health,
    ...modules.recovery,
    ...modules.resources
  });
});
