const assert = require("assert");
const contract = require("./shared/inventory/contract");
const api = require("./backend/api-routes");
const services = require("./backend/services");

let failures = 0;

async function run(name, test) {
  try {
    await test();
    console.log(`ok - ${name}`);
  } catch (error) {
    failures += 1;
    console.error(`FAIL - ${name}`);
    console.error(error);
  }
}

function legacyInventory() {
  return {
    devices: [{
      id: "edge-01",
      name: "North Edge",
      type: "NVR",
      host: "10.0.0.10",
      port: 80,
      channelCount: 2,
      status: "online"
    }],
    cameras: [{
      camera_id: "north-entry",
      device_id: "edge-01",
      displayName: "North Entry",
      channelNumber: 1,
      status: "online"
    }],
    groups: [{
      group_id: "operator-favorites",
      name: "Operator Favorites",
      preferred_grid: 4,
      camera_ids: ["north-entry", "missing-camera"]
    }]
  };
}

async function main() {
  await run("the browser/API/service composition imports one canonical inventory contract", () => {
    assert.strictEqual(api.INVENTORY_VERSION, contract.INVENTORY_VERSION);
    assert.strictEqual(api.normalizeInventory, contract.normalizeInventory);
    assert.strictEqual(services.deviceGroupId, contract.deviceGroupId);
    assert.strictEqual(services.normalizeDeviceType, contract.normalizeDeviceType);
    assert.strictEqual(services.normalizeDeviceChannelCount, contract.normalizeDeviceChannelCount);
  });

  await run("version 1 aliases upgrade deterministically to the canonical model", () => {
    const normalized = contract.normalizeInventory(legacyInventory());
    assert.strictEqual(normalized.version, 2);
    assert.strictEqual(normalized.devices[0].channels, 2);
    assert.strictEqual(normalized.devices[0].channelCount, 2);
    assert.deepStrictEqual(
      normalized.cameras.map(({ id, deviceId, name, channel }) => ({ id, deviceId, name, channel })),
      [{ id: "north-entry", deviceId: "edge-01", name: "North Entry", channel: 1 }]
    );
    assert.deepStrictEqual(normalized.groups.map((group) => group.id), [
      "operator-favorites",
      "grp-device-edge-01"
    ]);
    assert.deepStrictEqual(normalized.groups[0].cameraIds, ["north-entry"]);
    assert.deepStrictEqual(normalized.groups[1].cameraIds, ["north-entry"]);
  });

  await run("future inventory versions fail closed", () => {
    assert.throws(
      () => contract.normalizeInventory({ version: 3, devices: [], cameras: [], groups: [] }),
      /Unsupported inventory version/
    );
  });

  await run("stable channel and group identifiers are derived only from the device id", () => {
    const before = { id: "edge-01", name: "North Edge" };
    const after = { id: "edge-01", name: "Renamed Edge" };
    assert.strictEqual(contract.channelCameraId(before, 7), "edge-01-CH07");
    assert.strictEqual(contract.channelCameraId(after, 7), "edge-01-CH07");
    assert.strictEqual(contract.deviceGroupId(before), "grp-device-edge-01");
    assert.strictEqual(contract.deviceGroupId(after), "grp-device-edge-01");
  });

  await run("channel reduction removes placeholders, detaches configured cameras, and prunes references", () => {
    const device = {
      id: "edge-01",
      name: "North Edge",
      type: "NVR",
      channels: 1
    };
    const result = contract.reconcileDeviceChannels(device, [
      {
        id: "edge-01-CH01",
        deviceId: "edge-01",
        nvr: "Old Name",
        channel: 1,
        displayName: "Lobby Custom",
        name: "Lobby Custom",
        managedPlaceholder: false,
        deviceSyncManaged: true,
        related: ["edge-01-CH02"],
        next: ["configured-loading"],
        previous: []
      },
      {
        id: "edge-01-CH02",
        deviceId: "edge-01",
        nvr: "Old Name",
        channel: 2,
        managedPlaceholder: true,
        related: [],
        next: [],
        previous: []
      },
      {
        id: "configured-loading",
        deviceId: "edge-01",
        nvr: "Old Name",
        channel: 3,
        name: "Loading Dock",
        displayName: "Loading Dock",
        managedPlaceholder: false,
        related: [],
        next: [],
        previous: []
      }
    ]);

    assert.deepStrictEqual(result.removedIds, ["edge-01-CH02"]);
    assert.deepStrictEqual(result.detachedIds, ["configured-loading"]);
    const channelOne = result.cameras.find((camera) => camera.id === "edge-01-CH01");
    const detached = result.cameras.find((camera) => camera.id === "configured-loading");
    assert.strictEqual(channelOne.displayName, "Lobby Custom");
    assert.strictEqual(channelOne.nvr, "North Edge");
    assert.deepStrictEqual(channelOne.related, []);
    assert.deepStrictEqual(channelOne.next, ["configured-loading"]);
    assert.strictEqual(detached.deviceId, "");
    assert.strictEqual(detached.nvr, "Unassigned");
  });

  if (failures) {
    console.error(`\n${failures} inventory contract test(s) failed.`);
    process.exit(1);
  }
  console.log("\nAll canonical inventory contract tests passed.");
}

main();
