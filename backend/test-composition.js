const assert = require("assert");
const { createBackendComposition } = require("./composition");
const { FileDatabase } = require("./file-db");
const {
  CameraRepository,
  DeviceRepository,
  InventoryRepository
} = require("./repositories");

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

function memoryComposition() {
  const db = new FileDatabase("unused-composition-test.json");
  db.memoryOnly = true;
  return createBackendComposition({ db });
}

run("the composition root creates one real repository family", () => {
  const composition = memoryComposition();
  assert.ok(composition.repositories.devices instanceof DeviceRepository);
  assert.ok(composition.repositories.cameras instanceof CameraRepository);
  assert.ok(composition.repositories.inventory instanceof InventoryRepository);
  composition.repositories.devices.save({ id: "device-1", name: "Repository Device" });
  assert.strictEqual(composition.repositories.devices.findById("device-1").name, "Repository Device");
});

run("onboarding and commands reuse the root service and policy instances", () => {
  const { resiliencePolicy, services } = memoryComposition();
  assert.strictEqual(services.onboarding.devices, services.devices);
  assert.strictEqual(services.onboarding.cameras, services.cameras);
  assert.strictEqual(services.commands.services.mediaGuard, services.guard);
  assert.strictEqual(services.commands.resiliencePolicy, resiliencePolicy);
  assert.strictEqual(services.deviceIntegration.resiliencePolicy, resiliencePolicy);
});

run("canonical services persist only through injected repositories", () => {
  const { repositories, services } = memoryComposition();
  assert.strictEqual(Object.hasOwn(services.devices, "db"), false);
  assert.strictEqual(Object.hasOwn(services.cameras, "db"), false);
  assert.strictEqual(Object.hasOwn(services.guard, "db"), false);
  const result = services.onboarding.onboard({
    id: "device-2",
    name: "Composed Recorder",
    type: "NVR",
    host: "10.0.0.2",
    port: 80,
    channelCount: 2
  });
  assert.strictEqual(repositories.devices.findById("device-2").id, result.device.id);
  assert.deepStrictEqual(repositories.cameras.listByDevice("device-2").map((camera) => camera.id),
    result.cameras.map((camera) => camera.id));
});

if (failures) {
  console.error(`\n${failures} composition test(s) failed.`);
  process.exitCode = 1;
} else {
  console.log("\nAll repository composition tests passed.");
}
