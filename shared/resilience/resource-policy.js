(function resourcePolicyModule(root, factory) {
  const contract = typeof module === "object" && module.exports
    ? require("./contract")
    : root.VmsResilienceContract;
  const api = factory(contract);
  if (typeof module === "object" && module.exports) {
    module.exports = api;
  } else {
    root.VmsResilienceResources = api;
  }
})(typeof globalThis !== "undefined" ? globalThis : this, function createResourcePolicy(contract) {
  "use strict";

  const { WORK_PRIORITIES } = contract;

  const DEGRADATION_STAGES = Object.freeze([
    "normal",
    "pause-idle",
    "slow-low-polling",
    "stop-background-visuals",
    "pause-transfers",
    "reduce-eligible-media",
    "reject-new-media"
  ]);

  const DEGRADATION_ACTIONS = Object.freeze({
    normal: Object.freeze([]),
    "pause-idle": Object.freeze(["pause-idle-work"]),
    "slow-low-polling": Object.freeze(["pause-idle-work", "slow-low-priority-polling"]),
    "stop-background-visuals": Object.freeze([
      "pause-idle-work",
      "slow-low-priority-polling",
      "stop-thumbnails-and-background-discovery"
    ]),
    "pause-transfers": Object.freeze([
      "pause-idle-work",
      "slow-low-priority-polling",
      "stop-thumbnails-and-background-discovery",
      "pause-or-reschedule-downloads"
    ]),
    "reduce-eligible-media": Object.freeze([
      "pause-idle-work",
      "slow-low-priority-polling",
      "stop-thumbnails-and-background-discovery",
      "pause-or-reschedule-downloads",
      "reduce-eligible-live-stream-quality"
    ]),
    "reject-new-media": Object.freeze([
      "pause-idle-work",
      "slow-low-priority-polling",
      "stop-thumbnails-and-background-discovery",
      "pause-or-reschedule-downloads",
      "reduce-eligible-live-stream-quality",
      "reject-new-media-work-clearly"
    ])
  });

  const DEFAULT_WORK_CLASSIFICATIONS = Object.freeze({
    "alarm.intake": "system-critical",
    "audit.persist": "system-critical",
    "site.control": "system-critical",
    "recovery.coordinate": "system-critical",
    "media.live-selected": "high",
    "incident.investigation": "high",
    "media.playback-requested": "medium",
    "media.download-requested": "medium",
    "search.requested": "medium",
    "thumbnail.refresh": "low",
    "device.discovery": "low",
    "capability.refresh": "low",
    "metadata.background": "low",
    "inventory.registered-only": "idle"
  });

  class ResourcePolicy {
    constructor(classifications = DEFAULT_WORK_CLASSIFICATIONS) {
      this.classifications = new Map(Object.entries(classifications));
    }

    classify(workType) {
      const priority = this.classifications.get(String(workType || "")) || "low";
      return WORK_PRIORITIES.includes(priority) ? priority : "low";
    }

    replace(workType, priority) {
      if (!WORK_PRIORITIES.includes(priority)) {
        throw new RangeError(`Unsupported work priority: ${priority}`);
      }
      const next = new ResourcePolicy(Object.fromEntries(this.classifications));
      next.classifications.set(String(workType || ""), priority);
      return next;
    }

    sort(items = []) {
      const rank = new Map(WORK_PRIORITIES.map((priority, index) => [priority, index]));
      return items
        .map((item, index) => ({
          ...item,
          priority: item.priority || this.classify(item.workType),
          __index: index
        }))
        .sort((left, right) => {
          const priorityDifference = (rank.get(left.priority) ?? rank.get("low")) -
            (rank.get(right.priority) ?? rank.get("low"));
          if (priorityDifference !== 0) return priorityDifference;
          return left.__index - right.__index;
        })
        .map(({ __index, ...item }) => item);
    }
  }

  function degradationPlan(stage) {
    const normalized = DEGRADATION_STAGES.includes(stage) ? stage : "normal";
    return Object.freeze({
      stage: normalized,
      actions: DEGRADATION_ACTIONS[normalized],
      preservesSystemCriticalCapacity: true
    });
  }

  function manualOverrideDecision({
    hardLimitExceeded = false,
    displacesSystemCritical = false
  } = {}) {
    if (hardLimitExceeded) {
      return Object.freeze({ allowed: false, reasonCode: "HARD_RESOURCE_LIMIT" });
    }
    if (displacesSystemCritical) {
      return Object.freeze({ allowed: false, reasonCode: "SYSTEM_CRITICAL_CAPACITY_RESERVED" });
    }
    return Object.freeze({ allowed: true, reasonCode: "" });
  }

  return Object.freeze({
    DEFAULT_WORK_CLASSIFICATIONS,
    DEGRADATION_ACTIONS,
    DEGRADATION_STAGES,
    ResourcePolicy,
    degradationPlan,
    manualOverrideDecision
  });
});
