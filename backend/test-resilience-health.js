const assert = require("assert");
const {
  applyParentFailureToTree,
  confirmedNotRecording,
  createHealthSnapshot,
  createRootCause,
  deriveDependentSnapshot,
  summarizeHealth
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

run("reachability, freshness, function, recording, storage and adapter stay separate", () => {
  const health = createHealthSnapshot({
    resourceId: "recorder-1",
    reachability: "reachable",
    freshness: "current",
    functional: "degraded",
    recording: "recording",
    storage: "degraded",
    adapter: "available"
  });
  assert.strictEqual(health.reachability, "reachable");
  assert.strictEqual(health.recording, "recording");
  assert.strictEqual(health.storage, "degraded");
  assert.strictEqual(summarizeHealth(health).state, "degraded");
});

run("an unreachable recorder makes child evidence unknown and stale, not not-recording", () => {
  const child = deriveDependentSnapshot({
    resourceId: "recorder-1",
    reachability: "unreachable",
    freshness: "current"
  }, {
    resourceId: "camera-1",
    parentId: "recorder-1",
    reachability: "reachable",
    freshness: "current",
    functional: "healthy",
    recording: "recording",
    storage: "not-applicable"
  });
  assert.strictEqual(child.reachability, "unknown");
  assert.strictEqual(child.freshness, "stale");
  assert.strictEqual(child.recording, "unknown");
  assert.strictEqual(child.reasonCode, "PARENT_UNREACHABLE");
  assert.match(summarizeHealth(child).label, /current status unknown/i);
});

run("a parent failure changes only its direct dependants", () => {
  const resources = applyParentFailureToTree([
    { resourceId: "recorder-1", reachability: "unreachable", freshness: "current" },
    { resourceId: "camera-1", parentId: "recorder-1", reachability: "reachable", freshness: "current" },
    { resourceId: "camera-2", parentId: "recorder-2", reachability: "reachable", freshness: "current" }
  ], "recorder-1");
  assert.strictEqual(resources.find((item) => item.resourceId === "camera-1").reachability, "unknown");
  assert.strictEqual(resources.find((item) => item.resourceId === "camera-2").reachability, "reachable");
});

run("root-cause reporting produces one alarm with deterministic affected resources", () => {
  const root = createRootCause({
    id: "root-1",
    resourceId: "site-network-1",
    reasonCode: "SITE_NETWORK_UNREACHABLE",
    affectedResourceIds: ["camera-2", "camera-1", "camera-2"]
  });
  assert.strictEqual(root.alarmCount, 1);
  assert.deepStrictEqual(root.affectedResourceIds, ["camera-1", "camera-2"]);
});

run("not-recording requires current direct evidence", () => {
  assert.strictEqual(confirmedNotRecording({
    recording: "not-recording",
    freshness: "current",
    reasonCode: "RECORDER_REPORTED_STOPPED"
  }), true);
  assert.strictEqual(confirmedNotRecording({
    recording: "not-recording",
    freshness: "stale",
    reasonCode: "RECORDER_REPORTED_STOPPED"
  }), false);
});

run("unverified adapter support is unknown, not a device reachability failure", () => {
  const health = createHealthSnapshot({
    resourceId: "device-1",
    reachability: "unknown",
    freshness: "unknown",
    functional: "unknown",
    adapter: "unverified",
    reasonCode: "OPERATION_NOT_VERIFIED"
  });
  assert.strictEqual(summarizeHealth(health).state, "unknown");
  assert.notStrictEqual(health.reachability, "unreachable");
});

if (process.exitCode) {
  console.error("\nSome resilience health tests failed.");
} else {
  console.log("\nAll resilience health tests passed.");
}
