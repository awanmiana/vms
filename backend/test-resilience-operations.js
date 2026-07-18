const assert = require("assert");
const {
  OPERATION_IDS: O,
  assessDeliveryOutcome,
  createDefaultOperationPolicyRegistry,
  downloadResumeDecision,
  retryDecision
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

run("physical and destructive operations can never be queued or replayed", () => {
  const registry = createDefaultOperationPolicyRegistry();
  [
    O.PTZ,
    O.ACCESS_UNLOCK,
    O.BARRIER_OPEN,
    O.RELAY_ACTIVATE,
    O.BROADCAST,
    O.REBOOT,
    O.SHUTDOWN,
    O.FORMAT_STORAGE,
    O.DELETE_RECORDING,
    O.CHANGE_PASSWORD,
    O.UPDATE_FIRMWARE
  ].forEach((operationId) => {
    const policy = registry.resolve(operationId);
    assert.strictEqual(policy.queueMode, "none", operationId);
    assert.strictEqual(policy.mayReplay, false, operationId);
    assert.strictEqual(policy.requiresExpiry, true, operationId);
  });
});

run("expired physical commands are blocked before dispatch", () => {
  const registry = createDefaultOperationPolicyRegistry();
  const result = registry.evaluate(O.PTZ, {
    now: "2026-07-17T10:00:10Z",
    expiresAt: "2026-07-17T10:00:00Z",
    authorizationAvailable: true,
    durableAuditAvailable: true
  });
  assert.strictEqual(result.allowed, false);
  assert.strictEqual(result.outcome, "expired");
  assert.strictEqual(result.reasonCode, "COMMAND_EXPIRED");
});

run("audit failure uses the configurable secure default for privileged actions", () => {
  const registry = createDefaultOperationPolicyRegistry();
  const context = {
    now: "2026-07-17T10:00:00Z",
    expiresAt: "2026-07-17T10:00:30Z",
    authorizationAvailable: true,
    durableAuditAvailable: false
  };
  assert.strictEqual(registry.evaluate(O.PTZ, context).reasonCode, "DURABLE_AUDIT_REQUIRED");
  assert.strictEqual(registry.evaluate(O.PTZ, {
    ...context,
    auditFailureMode: "allow-with-critical-warning"
  }).allowed, true);
  assert.strictEqual(registry.evaluate(O.LIVE_VIEW, context).allowed, true);
});

run("lost confirmation after transmitting a physical command is outcome unknown", () => {
  const result = assessDeliveryOutcome(O.ACCESS_UNLOCK, {
    transmitted: true,
    confirmed: false
  });
  assert.strictEqual(result.outcome, "outcome-unknown");
  assert.strictEqual(result.mayReplay, false);
});

run("safe reads may retry temporary errors but unsafe commands may not", () => {
  assert.strictEqual(retryDecision({
    operationId: O.DEVICE_HEALTH,
    errorCategory: "network-temporary"
  }).retry, true);
  assert.strictEqual(retryDecision({
    operationId: O.PTZ,
    errorCategory: "timeout"
  }).retry, false);
  assert.strictEqual(retryDecision({
    operationId: O.DEVICE_HEALTH,
    errorCategory: "authentication"
  }).retry, false);
});

run("versioned mutations require an idempotency key and expected version", () => {
  assert.strictEqual(retryDecision({
    operationId: O.INVENTORY_WRITE,
    errorCategory: "server-temporary"
  }).retry, false);
  assert.strictEqual(retryDecision({
    operationId: O.INVENTORY_WRITE,
    errorCategory: "server-temporary",
    idempotencyKey: "write-1",
    expectedVersion: 7
  }).retry, true);
});

run("downloads resume only with range support and a stable source validator", () => {
  assert.strictEqual(downloadResumeDecision({
    partialBytes: 100,
    rangeSupported: true,
    stableSourceValidator: true,
    sourceDigestAvailable: false
  }).action, "resume");
  assert.strictEqual(downloadResumeDecision({
    partialBytes: 100,
    rangeSupported: true,
    stableSourceValidator: false
  }).action, "restart");
  assert.strictEqual(downloadResumeDecision({
    partialBytes: 100,
    rangeSupported: true,
    stableSourceValidator: true,
    sourceDigestAvailable: false
  }).sourceEqualityVerifiable, false);
});

run("unknown operations are restricted rather than assumed safe", () => {
  const policy = createDefaultOperationPolicyRegistry().resolve("future.operation");
  assert.strictEqual(policy.safetyClass, "restricted");
  assert.strictEqual(policy.retryMode, "never");
  assert.strictEqual(policy.requiresDurableAudit, true);
});

if (process.exitCode) {
  console.error("\nSome resilience operation tests failed.");
} else {
  console.log("\nAll resilience operation tests passed.");
}
