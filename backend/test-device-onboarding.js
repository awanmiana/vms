const assert = require("assert");
const path = require("path");
const { FileDatabase } = require("./file-db");
const { CameraService, DeviceOnboardingService, DeviceService, deviceGroupId } = require("./services");

function freshDb() {
  const db = new FileDatabase(path.join(__dirname, "test-device-onboarding-unused.json"));
  db.memoryOnly = true;
  db.load();
  return db;
}

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

function device(overrides = {}) {
  return {
    id: "dev-test",
    name: "Test Recorder",
    type: "NVR",
    vendor: "Test Vendor",
    host: "192.168.50.10",
    port: 8000,
    channelCount: 2,
    status: "online",
    ...overrides
  };
}

run("onboarding creates a deterministic system group for every supported device type", () => {
  const db = freshDb();
  const onboarding = new DeviceOnboardingService(db);
  const types = ["NVR", "DVR", "Hybrid DVR", "IP Camera Direct"];

  types.forEach((type, index) => {
    const requestedChannelCount = type === "IP Camera Direct" ? 16 : 2;
    const expectedChannelCount = type === "IP Camera Direct" ? 1 : requestedChannelCount;
    const result = onboarding.onboard(
      device({
        id: `dev-type-${index + 1}`,
        name: `${type} ${index + 1}`,
        type,
        host: `192.168.50.${index + 1}`,
        channelCount: requestedChannelCount
      })
    );

    assert.strictEqual(result.device.type, type);
    assert.strictEqual(result.device.channelCount, expectedChannelCount);
    assert.strictEqual(result.cameras.length, expectedChannelCount);
    assert.strictEqual(result.group.id, `grp-device-${result.device.id}`);
    assert.strictEqual(result.group.deviceId, result.device.id);
    assert.strictEqual(result.group.system, true);
    assert.deepStrictEqual(result.group.cameraIds, result.cameras.map((camera) => camera.id));
  });

  assert.strictEqual(db.table("groups").length, types.length);
});

run("onboarding and channel sync are idempotent", () => {
  const db = freshDb();
  const onboarding = new DeviceOnboardingService(db);
  const input = device({ id: "dev-idempotent", name: "Idempotent NVR", channelCount: 2 });

  const first = onboarding.onboard(input);
  const second = onboarding.onboard(input);
  const syncedAgain = onboarding.syncDevice(input.id);

  assert.strictEqual(db.table("devices").length, 1);
  assert.strictEqual(db.table("cameras").length, 2);
  assert.strictEqual(db.table("streamProfiles").length, 6);
  assert.strictEqual(db.table("groups").length, 1);
  assert.deepStrictEqual(second.cameras.map((camera) => camera.id), first.cameras.map((camera) => camera.id));
  assert.deepStrictEqual(syncedAgain.group.cameraIds, first.group.cameraIds);
  assert.strictEqual(new Set(syncedAgain.group.cameraIds).size, 2);
});

run("generated camera status stays unknown when parent recorder status is offline or unverified", () => {
  const db = freshDb();
  const onboarding = new DeviceOnboardingService(db);

  ["offline", "unknown"].forEach((status, index) => {
    const result = onboarding.onboard(device({
      id: `dev-parent-${index + 1}`,
      name: `Parent ${status}`,
      status,
      channelCount: 2
    }));

    assert.strictEqual(result.device.status, status);
    assert.deepStrictEqual(
      result.cameras.map((camera) => camera.status),
      ["unknown", "unknown"],
      "parent reachability is not direct evidence about child camera state"
    );
  });
});

run("device rename updates managed names and group identity while preserving manual camera metadata", () => {
  const db = freshDb();
  const onboarding = new DeviceOnboardingService(db);
  const first = onboarding.onboard(device({ id: "dev-rename", name: "Old Recorder", channelCount: 2 }));
  const originalCameraIds = first.cameras.map((camera) => camera.id);
  const manualCamera = first.cameras[0];

  db.upsert("cameras", {
    ...manualCamera,
    displayName: "Lobby Entrance",
    area: "Main Lobby",
    floor: "Ground",
    direction: "North",
    tags: ["priority", "entrance"],
    operatorNote: "Keep this label"
  });

  const renamed = onboarding.onboard(device({ id: "dev-rename", name: "Renamed Recorder", channelCount: 2 }));
  const preserved = renamed.cameras.find((camera) => camera.channelNumber === 1);
  const managed = renamed.cameras.find((camera) => camera.channelNumber === 2);

  assert.strictEqual(renamed.group.id, deviceGroupId("dev-rename"));
  assert.strictEqual(renamed.group.name, "Renamed Recorder (Assigned)");
  assert.deepStrictEqual(renamed.group.cameraIds, originalCameraIds);
  assert.strictEqual(preserved.displayName, "Lobby Entrance");
  assert.strictEqual(preserved.area, "Main Lobby");
  assert.strictEqual(preserved.floor, "Ground");
  assert.strictEqual(preserved.direction, "North");
  assert.deepStrictEqual(preserved.tags, ["priority", "entrance"]);
  assert.strictEqual(preserved.operatorNote, "Keep this label");
  assert.strictEqual(managed.displayName, "Renamed Recorder CH-02");
});

run("channel-count reconciliation removes stale rows and repairs deterministic membership", () => {
  const db = freshDb();
  const onboarding = new DeviceOnboardingService(db);
  const threeChannels = device({ id: "dev-resize", name: "Resize DVR", type: "DVR", channelCount: 3 });
  const initial = onboarding.onboard(threeChannels);
  const initialIds = initial.cameras.map((camera) => camera.id);

  db.upsert("cameras", { ...initial.cameras[0], area: "Protected Area", tags: ["retain"] });
  db.upsert("groups", {
    id: "grp-custom",
    name: "Custom Cross-device Group",
    cameraIds: initialIds.slice(),
    system: false
  });

  const reduced = onboarding.onboard({ ...threeChannels, channelCount: 1 });

  assert.strictEqual(reduced.cameras.length, 1);
  assert.deepStrictEqual(reduced.group.cameraIds, [initialIds[0]]);
  assert.strictEqual(reduced.cameras[0].area, "Protected Area");
  assert.deepStrictEqual(reduced.cameras[0].tags, ["retain"]);
  assert.deepStrictEqual(db.table("groups").find((group) => group.id === "grp-custom").cameraIds, [initialIds[0]]);
  assert.strictEqual(db.table("streamProfiles").length, 3);
  assert.strictEqual(db.table("streamProfiles").some((profile) => initialIds.slice(1).includes(profile.cameraId)), false);

  const expanded = onboarding.onboard(threeChannels);

  assert.deepStrictEqual(expanded.cameras.map((camera) => camera.id), initialIds);
  assert.deepStrictEqual(expanded.group.cameraIds, initialIds);
  assert.deepStrictEqual(expanded.cameras.map((camera) => camera.channelNumber), [1, 2, 3]);
  assert.strictEqual(db.table("streamProfiles").length, 9);
  assert.strictEqual(expanded.cameras[0].area, "Protected Area");
});

run("direct camera sync also guarantees the device group", () => {
  const db = freshDb();
  const devices = new DeviceService(db);
  const cameras = new CameraService(db);
  const saved = devices.save(device({ id: "dev-direct-sync", name: "Direct IP Camera", type: "IP Camera Direct", channelCount: 1 }));

  const emptyGroup = cameras.deviceGroup(saved);
  assert.strictEqual(emptyGroup.id, "grp-device-dev-direct-sync");
  assert.deepStrictEqual(emptyGroup.cameraIds, [], "saving a device alone must still create its system group");

  const synced = cameras.syncDeviceChannels(saved);
  const group = cameras.deviceGroup(saved);

  assert.strictEqual(synced.length, 1);
  assert.strictEqual(group.id, "grp-device-dev-direct-sync");
  assert.deepStrictEqual(group.cameraIds, [synced[0].id]);
});

run("unsupported device types are rejected before persistence", () => {
  const db = freshDb();
  const onboarding = new DeviceOnboardingService(db);

  assert.throws(
    () => onboarding.onboard(device({ type: "Unknown Recorder" })),
    /Unsupported device type/
  );
  assert.strictEqual(db.table("devices").length, 0);
  assert.strictEqual(db.table("groups").length, 0);
});

run("device persistence requires an explicit vendor-neutral port", () => {
  const db = freshDb();
  const devices = new DeviceService(db);

  assert.throws(
    () => devices.save(device({ port: undefined })),
    /explicit integer from 1 to 65535/
  );
  assert.strictEqual(db.table("devices").length, 0);
});

if (process.exitCode) {
  console.error("\nSome device onboarding tests failed.");
} else {
  console.log("\nAll device onboarding tests passed.");
}
