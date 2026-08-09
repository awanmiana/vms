const assert = require("assert");
const { COMMAND_OPERATION_IDS, CommandExecutor } = require("./commands");
const { DEVICE_OPERATIONS } = require("./device-integration");
const { FileDatabase } = require("./file-db");
const {
  OPERATION_IDS,
  ResiliencePolicyService,
  adapterOutcome,
  commandOutcome,
  httpStatusForOutcome,
  persistenceOutcome,
  policyOutcome
} = require("./resilience");

let failures = 0;

function run(name, test) {
  try {
    test();
    console.log(`ok - ${name}`);
  } catch (error) {
    failures += 1;
    console.error(`FAIL - ${name}`);
    console.error(error);
  }
}

run("device and command surfaces reference the canonical operation registry", () => {
  assert.deepStrictEqual(DEVICE_OPERATIONS, {
    DISCOVERY: OPERATION_IDS.DEVICE_DISCOVERY,
    HEALTH: OPERATION_IDS.DEVICE_HEALTH,
    FIRMWARE: OPERATION_IDS.DEVICE_FIRMWARE,
    EVENTS: OPERATION_IDS.DEVICE_EVENTS,
    PTZ: OPERATION_IDS.PTZ
  });
  assert.strictEqual(COMMAND_OPERATION_IDS.OPEN_CAMERA, OPERATION_IDS.LIVE_VIEW);
  assert.strictEqual(COMMAND_OPERATION_IDS.PAN_TO_ZONE, OPERATION_IDS.PTZ);
  assert.strictEqual(COMMAND_OPERATION_IDS.START_TRACKING, OPERATION_IDS.TRACKING_UPDATE);
  const registered = new Set(new ResiliencePolicyService().operationPolicies.list().map((policy) => policy.id));
  Object.values(DEVICE_OPERATIONS).forEach((operationId) => assert.ok(registered.has(operationId)));
  Object.values(COMMAND_OPERATION_IDS).forEach((operationId) => assert.ok(registered.has(operationId)));
});

run("adapter statuses map explicitly to canonical outcomes", () => {
  assert.deepStrictEqual([
    adapterOutcome({ status: "succeeded" }),
    adapterOutcome({ status: "failed" }),
    adapterOutcome({ status: "unsupported" }),
    adapterOutcome({ status: "unknown" }),
    adapterOutcome({ status: "unavailable" }),
    adapterOutcome({ status: "unknown", outcomeUnknown: true })
  ], ["succeeded", "failed", "unsupported", "unknown", "unavailable", "outcome-unknown"]);
});

run("policy, command, and persistence boundaries have explicit outcome maps", () => {
  assert.deepStrictEqual(
    ["allowed", "blocked", "expired", "outcome-unknown", "confirmed", "not-transmitted"]
      .map((outcome) => policyOutcome({ outcome })),
    ["allowed", "blocked", "expired", "outcome-unknown", "succeeded", "failed"]
  );
  assert.deepStrictEqual(
    ["executed", "rejected", "needs_confirmation"].map((status) => commandOutcome({ status })),
    ["succeeded", "failed", "blocked"]
  );
  assert.strictEqual(persistenceOutcome({ code: "PERSISTENCE_UNAVAILABLE" }), "unavailable");
  assert.strictEqual(persistenceOutcome({ code: "PERSISTENCE_SCHEMA_INVALID" }), "failed");
});

run("canonical outcomes map explicitly onto HTTP transport status", () => {
  assert.deepStrictEqual(
    ["allowed", "succeeded", "failed", "blocked", "expired", "unsupported", "unknown", "unavailable", "outcome-unknown"]
      .map(httpStatusForOutcome),
    [200, 200, 500, 403, 409, 501, 503, 503, 502]
  );
});

run("the command executor blocks a physical operation before dispatch when expiry is absent", () => {
  const db = new FileDatabase("unused-operation-vocabulary.json");
  db.memoryOnly = true;
  const result = new CommandExecutor(db).execute({
    intent: "PAN_TO_ZONE",
    target: { type: "camera", id: "cam-1" },
    source: "ui_click"
  });
  assert.strictEqual(result.operationId, OPERATION_IDS.PTZ);
  assert.strictEqual(result.outcome, "blocked");
  assert.strictEqual(result.reasonCode, "COMMAND_EXPIRY_REQUIRED");
});

if (failures) {
  console.error(`\n${failures} operation vocabulary test(s) failed.`);
  process.exitCode = 1;
} else {
  console.log("\nAll operation vocabulary tests passed.");
}
