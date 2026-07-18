const assert = require("assert");
const path = require("path");
const { FileDatabase, FileDatabasePersistenceError } = require("./file-db");
const {
  DEPENDENCY_IDS: D,
  FUNCTION_IDS: F,
  OPERATION_IDS: O,
  AvailabilityPolicyRegistry,
  ResiliencePolicyService,
  createDefaultAvailabilityPolicyRegistry
} = require("./resilience");

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

run("the backend facade composes replaceable policy providers", () => {
  const defaults = createDefaultAvailabilityPolicyRegistry();
  const customAvailability = new AvailabilityPolicyRegistry(defaults.list()).replace({
    id: F.LIVE_VIEW,
    requiredAll: [D.SOURCE_DEVICE]
  });
  const service = new ResiliencePolicyService({ availabilityPolicies: customAvailability });
  assert.strictEqual(service.functionAvailability(F.LIVE_VIEW, {
    [D.SOURCE_DEVICE]: "available"
  }).allowed, true);
});

run("the secure audit default can be replaced without changing callers", () => {
  const context = {
    now: "2026-07-17T10:00:00Z",
    expiresAt: "2026-07-17T10:00:30Z",
    authorizationAvailable: true,
    durableAuditAvailable: false
  };
  assert.strictEqual(new ResiliencePolicyService().operationPreflight(O.PTZ, context).allowed, false);
  assert.strictEqual(new ResiliencePolicyService({
    auditFailureMode: "allow-with-critical-warning"
  }).operationPreflight(O.PTZ, context).allowed, true);
});

run("the facade keeps delivery uncertainty and replay safety explicit", () => {
  const outcome = new ResiliencePolicyService().deliveryOutcome(O.PTZ, {
    transmitted: true,
    confirmed: false
  });
  assert.strictEqual(outcome.outcome, "outcome-unknown");
  assert.strictEqual(outcome.mayReplay, false);
});

run("automatic persistence fallback cannot report a successful service mutation", () => {
  const db = new FileDatabase(path.join(__dirname, "unused-resilience-fallback.json"));
  db.memoryOnly = true;
  db.persistenceFallback = {
    code: "ENOENT",
    message: "The durable store is unavailable."
  };
  assert.throws(
    () => db.upsert("devices", { id: "unsaved-device" }),
    (error) => error instanceof FileDatabasePersistenceError &&
      error.code === "PERSISTENCE_UNAVAILABLE"
  );
  assert.strictEqual(db.table("devices").length, 0);
});

if (process.exitCode) {
  console.error("\nSome resilience policy service tests failed.");
} else {
  console.log("\nAll resilience policy service tests passed.");
}
