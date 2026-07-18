const assert = require("assert");
const {
  AvailabilityPolicyRegistry,
  DEPENDENCY_IDS: D,
  FUNCTION_IDS: F,
  createDefaultAvailabilityPolicyRegistry
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

run("native device operation and recording remain outside the VMS failure path", () => {
  const registry = createDefaultAvailabilityPolicyRegistry();
  const failedVmsDependencies = {
    [D.UI_RUNTIME]: "unavailable",
    [D.LOCAL_RUNTIME]: "unavailable",
    [D.CONTINUITY_PROVIDER]: "unavailable",
    [D.WAN]: "unavailable",
    [D.COORDINATOR]: "unavailable",
    [D.INVENTORY_AUTHORITY]: "unavailable",
    [D.VENDOR_SERVICE]: "unavailable",
    [D.LICENSED_COMPONENT]: "unavailable"
  };
  const operation = registry.evaluate(F.NATIVE_DEVICE_OPERATION, failedVmsDependencies);
  const recording = registry.evaluate(F.NATIVE_RECORDING, failedVmsDependencies);
  assert.strictEqual(operation.mode, "independent");
  assert.strictEqual(operation.sourceOperationUnaffected, true);
  assert.strictEqual(recording.mode, "independent");
  assert.strictEqual(recording.sourceOperationUnaffected, true);
});

run("authority loss makes cached inventory explicitly stale and read-only", () => {
  const registry = createDefaultAvailabilityPolicyRegistry();
  const read = registry.evaluate(F.INVENTORY_READ, {
    [D.INVENTORY_AUTHORITY]: "unavailable",
    [D.INVENTORY_CACHE]: "available"
  });
  const write = registry.evaluate(F.INVENTORY_WRITE, {
    [D.INVENTORY_AUTHORITY]: "unavailable",
    [D.AUTHORIZATION]: "available"
  });
  assert.strictEqual(read.mode, "read-only");
  assert.strictEqual(read.allowed, true);
  assert.strictEqual(read.stale, true);
  assert.strictEqual(write.allowed, false);
  assert.strictEqual(write.reasonCode, "INVENTORY_AUTHORITY_REQUIRED");
});

run("WAN and coordinator loss do not block a source-local live view", () => {
  const registry = createDefaultAvailabilityPolicyRegistry();
  const dependencies = {
    [D.LOCAL_NETWORK]: "available",
    [D.SOURCE_DEVICE]: "available",
    [D.AUTHORIZATION]: "available",
    [D.WAN]: "unavailable",
    [D.COORDINATOR]: "unavailable"
  };
  assert.strictEqual(registry.evaluate(F.LIVE_VIEW, dependencies).mode, "available");
  assert.strictEqual(registry.evaluate(F.REMOTE_CROSS_SITE, dependencies).mode, "unavailable");
});

run("continuous VMS monitoring requires a continuity provider but devices remain independent", () => {
  const registry = createDefaultAvailabilityPolicyRegistry();
  const dependencies = {
    [D.CONTINUITY_PROVIDER]: "unavailable",
    [D.SOURCE_DEVICE]: "available"
  };
  assert.strictEqual(registry.evaluate(F.MONITORING_COLLECT, dependencies).allowed, false);
  assert.strictEqual(registry.evaluate(F.NATIVE_DEVICE_OPERATION, dependencies).sourceOperationUnaffected, true);
});

run("optional vendor and licence failures affect only functions declaring those dependencies", () => {
  const registry = createDefaultAvailabilityPolicyRegistry();
  const dependencies = {
    [D.LOCAL_NETWORK]: "available",
    [D.SOURCE_DEVICE]: "available",
    [D.AUTHORIZATION]: "available",
    [D.VENDOR_SERVICE]: "unavailable",
    [D.LICENSED_COMPONENT]: "unavailable"
  };
  assert.strictEqual(registry.evaluate(F.LOCAL_DEVICE_READ, dependencies).allowed, true);
  assert.strictEqual(registry.evaluate(F.VENDOR_SERVICE_OPERATION, dependencies).allowed, false);
  assert.strictEqual(registry.evaluate(F.LICENSED_COMPONENT_OPERATION, dependencies).allowed, false);
});

run("unknown functions fail conservatively and policies are replaceable per registry", () => {
  const defaults = createDefaultAvailabilityPolicyRegistry();
  const custom = new AvailabilityPolicyRegistry(defaults.list());
  custom.replace({
    id: F.LIVE_VIEW,
    requiredAll: [D.SOURCE_DEVICE]
  });
  assert.strictEqual(defaults.evaluate(F.LIVE_VIEW, { [D.SOURCE_DEVICE]: "available" }).allowed, false);
  assert.strictEqual(custom.evaluate(F.LIVE_VIEW, { [D.SOURCE_DEVICE]: "available" }).allowed, true);
  assert.strictEqual(custom.evaluate("future.unknown", {}).allowed, false);
});

if (process.exitCode) {
  console.error("\nSome resilience availability tests failed.");
} else {
  console.log("\nAll resilience availability tests passed.");
}
