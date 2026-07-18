const assert = require("assert");
const {
  DEGRADATION_STAGES,
  RECOVERY_PHASES,
  ResourcePolicy,
  calculateFullJitterDelay,
  degradationPlan,
  manualOverrideDecision,
  recoveryBudgetKey,
  sortRecoveryItems
} = require("../shared/resilience");

function run(name, fn) {
  try {
    fn();
    console.log(`ok - ${name}`);
  } catch (error) {
    console.error(`FAIL - ${name}`);
    console.error(error);
    process.exitCode = 1;
  }
}

run("work is ordered system-critical, high, medium, low and idle", () => {
  const policy = new ResourcePolicy();
  const sorted = policy.sort([
    { id: "idle", workType: "inventory.registered-only" },
    { id: "medium", workType: "media.download-requested" },
    { id: "critical", workType: "alarm.intake" },
    { id: "low", workType: "thumbnail.refresh" },
    { id: "high", workType: "media.live-selected" }
  ]);
  assert.deepStrictEqual(sorted.map((item) => item.id), ["critical", "high", "medium", "low", "idle"]);
});

run("resource policy overrides are instance-local and preserve defaults", () => {
  const defaults = new ResourcePolicy();
  const customized = defaults.replace("thumbnail.refresh", "medium");
  assert.strictEqual(defaults.classify("thumbnail.refresh"), "low");
  assert.strictEqual(customized.classify("thumbnail.refresh"), "medium");
});

run("degradation follows the approved order and preserves critical capacity", () => {
  assert.deepStrictEqual(DEGRADATION_STAGES, [
    "normal",
    "pause-idle",
    "slow-low-polling",
    "stop-background-visuals",
    "pause-transfers",
    "reduce-eligible-media",
    "reject-new-media"
  ]);
  const plan = degradationPlan("reduce-eligible-media");
  assert.strictEqual(plan.preservesSystemCriticalCapacity, true);
  assert.deepStrictEqual(plan.actions, [
    "pause-idle-work",
    "slow-low-priority-polling",
    "stop-thumbnails-and-background-discovery",
    "pause-or-reschedule-downloads",
    "reduce-eligible-live-stream-quality"
  ]);
});

run("manual tuning cannot exceed hard limits or displace critical work", () => {
  assert.strictEqual(manualOverrideDecision({ hardLimitExceeded: true }).allowed, false);
  assert.strictEqual(manualOverrideDecision({ displacesSystemCritical: true }).allowed, false);
  assert.strictEqual(manualOverrideDecision({}).allowed, true);
});

run("recovery follows policy phases without hard-coding one global scheduler", () => {
  const items = RECOVERY_PHASES.slice().reverse().map((phase) => ({ id: phase, phase }));
  assert.deepStrictEqual(sortRecoveryItems(items).map((item) => item.phase), RECOVERY_PHASES);
  assert.notStrictEqual(
    recoveryBudgetKey({ siteId: "site-a", dependencyId: "recorder-1" }),
    recoveryBudgetKey({ siteId: "site-b", dependencyId: "recorder-1" })
  );
});

run("full-jitter backoff is deterministic when policy inputs are injected", () => {
  assert.strictEqual(calculateFullJitterDelay({
    attempt: 3,
    baseMs: 100,
    capMs: 500,
    random: () => 0.5
  }), 250);
  assert.strictEqual(calculateFullJitterDelay({
    attempt: 10,
    baseMs: 100,
    capMs: 500,
    random: () => 1
  }), 500);
});

if (process.exitCode) {
  console.error("\nSome resilience recovery/priority tests failed.");
} else {
  console.log("\nAll resilience recovery/priority tests passed.");
}
