const assert = require("assert");
const backendDateTime = require("./backend/datetime");
const backendMediaPolicy = require("./backend/media-policy");
const core = require("./shared/core");
const mediaPolicy = require("./shared/media-policy");
const inventory = require("./shared/inventory/contract");
const resilience = require("./shared/resilience");

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

run("browser and Node import the same media and timestamp authorities", () => {
  assert.strictEqual(backendMediaPolicy, mediaPolicy);
  assert.strictEqual(backendDateTime.formatDateTime, core.formatDateTime);
  assert.strictEqual(core.formatDateTime(new Date(2026, 7, 9, 4, 5, 6)), "2026-08-09 04:05:06");
});

run("default stream profiles and bitrate estimates come from one source", () => {
  const profiles = mediaPolicy.defaultStreamProfiles("camera-1");
  assert.deepStrictEqual(profiles.map((profile) => profile.id), [
    "camera-1-thumb",
    "camera-1-sub",
    "camera-1-main"
  ]);
  profiles.forEach((profile) => {
    assert.strictEqual(profile.bitrateKbps, mediaPolicy.estimateBitrateKbps(profile.tier));
  });
});

run("tier choice and capacity guard share the canonical tier vocabulary", () => {
  assert.strictEqual(mediaPolicy.resolveTier({ tileCount: 9, isVisible: true }), "thumb");
  assert.strictEqual(mediaPolicy.resolveTierByZone({ zone: "offscreen", zoomLevel: "room" }), "paused");
  assert.strictEqual(mediaPolicy.canOpenTier({
    tier: "main",
    deviceUsage: { main: 4, sub: 0 },
    deviceLimits: { maxConcurrentMainstream: 4, maxConcurrentSubstream: 32 }
  }), false);
});

run("shared ids, stable ordering, validation errors, and policy registries remain explicit", () => {
  assert.match(core.createId("event"), /^event-[0-9a-f-]{36}$/);
  assert.throws(() => core.createId("Invalid Prefix!"), (error) =>
    error instanceof core.SharedContractError && error.code === "INVALID_ID_PREFIX");
  assert.deepStrictEqual(
    core.stableSort([{ id: "a", rank: 1 }, { id: "b", rank: 1 }], (a, b) => a.rank - b.rank)
      .map((item) => item.id),
    ["a", "b"]
  );
  assert.throws(
    () => inventory.normalizeInventory({ devices: [], cameras: [], groups: "invalid" }),
    (error) => error instanceof inventory.InventoryValidationError
  );
  assert.ok(resilience.createDefaultOperationPolicyRegistry().resolve(resilience.OPERATION_IDS.PTZ));
});

if (failures) {
  console.error(`\n${failures} shared primitive test(s) failed.`);
  process.exitCode = 1;
} else {
  console.log("\nAll shared primitive tests passed.");
}
