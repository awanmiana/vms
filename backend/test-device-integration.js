const assert = require("assert");
const path = require("path");
const { FileDatabase } = require("./file-db");
const { DeviceIntegrationService } = require("./device-integration");
const {
  ADAPTER_CONTRACT_VERSION,
  DeviceAdapterOperationError,
  DeviceAdapterRegistry,
  createDefaultDeviceAdapterRegistry,
  createDeviceAdapter
} = require("./device-adapters");

function freshDb() {
  const db = new FileDatabase(path.join(__dirname, "test-device-integration-unused.json"));
  db.memoryOnly = true;
  db.load();
  return db;
}

async function run(name, fn) {
  try {
    await fn();
    console.log(`ok - ${name}`);
  } catch (error) {
    console.error(`FAIL - ${name}`);
    console.error(error);
    process.exitCode = 1;
  }
}

function createFakeAdapter(id = "test-vendor", operationOverrides = {}) {
  return createDeviceAdapter({
    manifest: {
      contractVersion: ADAPTER_CONTRACT_VERSION,
      id,
      displayName: "Test Vendor",
      implementationStatus: "test-only"
    },
    operations: {
      "device.discovery": async () => [{
        id: "disc-1",
        vendor: "Test Vendor",
        host: "192.168.1.10",
        port: 12345,
        name: "Gate NVR"
      }],
      "device.health": async () => ({
        ok: true,
        latencyMs: 18,
        firmwareVersion: "V1.0.0",
        message: "reachable"
      }),
      "device.firmware": async () => ({
        currentVersion: "V1.0.0",
        latestKnownVersion: "V1.1.0",
        updateAvailable: true
      }),
      "device.events": async () => [{
        id: "evt-1",
        eventType: "motion",
        severity: "info",
        title: "Motion",
        message: "Channel 1"
      }],
      "camera.ptz": async () => ({ ok: true, message: "executed live" }),
      ...operationOverrides
    }
  });
}

function serviceWithFakeAdapter() {
  const registry = new DeviceAdapterRegistry().register(createFakeAdapter());
  return { db: freshDb(), registry };
}

async function main() {
  await run("discovery writes adapter-owned candidate rows without inventing a port", async () => {
    const { db, registry } = serviceWithFakeAdapter();
    const service = new DeviceIntegrationService(db, registry);
    const result = await service.discover({ range: "192.168.1.0/24", adapterId: "test-vendor" });

    assert.strictEqual(result.status, "succeeded");
    assert.strictEqual(result.devices.length, 1);
    assert.strictEqual(db.table("deviceDiscoveryResults")[0].adapterId, "test-vendor");
    assert.strictEqual(db.table("deviceDiscoveryResults")[0].port, 12345);
  });

  await run("health check stores live status evidence through an explicit adapter", async () => {
    const { db, registry } = serviceWithFakeAdapter();
    const service = new DeviceIntegrationService(db, registry);
    const result = await service.testConnection({ id: "dev-1", adapterId: "test-vendor" });

    assert.strictEqual(result.status, "succeeded");
    assert.strictEqual(result.health.status, "online");
    assert.strictEqual(result.health.latencyMs, 18);
  });

  await run("event ingestion writes normalized event rows through the adapter boundary", async () => {
    const { db, registry } = serviceWithFakeAdapter();
    const service = new DeviceIntegrationService(db, registry);
    const result = await service.ingestEvents({ id: "dev-1", adapterId: "test-vendor" });

    assert.strictEqual(result.status, "succeeded");
    assert.strictEqual(result.events.length, 1);
    assert.strictEqual(db.table("deviceEvents")[0].eventType, "motion");
  });

  await run("PTZ executes live and the backward queue alias never persists delayed work", async () => {
    const { db, registry } = serviceWithFakeAdapter();
    const service = new DeviceIntegrationService(db, registry);
    const result = await service.executePtzCommand(
      { id: "dev-1", adapterId: "test-vendor" },
      { id: "cam-1" },
      { action: "zoom-in", params: { speed: 3 } }
    );

    assert.strictEqual(result.status, "succeeded");
    assert.strictEqual(result.commandRecord.status, "succeeded");
    assert.strictEqual(result.commandRecord.executionMode, "live");
    assert.strictEqual(result.commandRecord.replayAllowed, false);
    assert.strictEqual(db.table("ptzCommandLog")[0].command, "zoom-in");

    const aliasResult = await service.queuePtzCommand(
      { id: "dev-1", adapterId: "test-vendor" },
      { id: "cam-1" },
      { action: "pan-left", params: { speed: 2 } }
    );
    assert.strictEqual(aliasResult.commandRecord.status, "succeeded");
    assert.strictEqual(db.table("ptzCommandLog").some((row) => row.status === "queued"), false);
    assert.strictEqual(db.table("ptzCommandLog").every((row) => row.replayAllowed === false), true);
  });

  await run("a post-send PTZ failure is recorded as outcome unknown and cannot be replayed", async () => {
    const db = freshDb();
    const registry = new DeviceAdapterRegistry().register(createFakeAdapter("uncertain", {
      "camera.ptz": async () => {
        throw new DeviceAdapterOperationError(
          "DEVICE_RESPONSE_LOST",
          "The command was sent but its response was lost.",
          { outcomeUnknown: true }
        );
      }
    }));
    const service = new DeviceIntegrationService(db, registry);
    const result = await service.executePtzCommand(
      { id: "dev-1", adapterId: "uncertain" },
      { id: "cam-1" },
      { action: "zoom-in", params: { speed: 3 } }
    );

    assert.strictEqual(result.status, "unknown");
    assert.strictEqual(result.outcomeUnknown, true);
    assert.strictEqual(result.commandRecord.status, "outcome_unknown");
    assert.strictEqual(result.commandRecord.replayAllowed, false);
    assert.strictEqual(db.table("ptzCommandLog").length, 1);
    assert.strictEqual(db.table("ptzCommandLog")[0].reasonCode, "DEVICE_RESPONSE_LOST");
  });

  await run("free-text vendor and device type never select an adapter", async () => {
    const db = freshDb();
    const service = new DeviceIntegrationService(db, new DeviceAdapterRegistry().register(createFakeAdapter("reference")));
    const result = await service.testConnection({ id: "dev-1", vendor: "Reference Vendor", type: "NVR" });

    assert.strictEqual(result.status, "unavailable");
    assert.strictEqual(result.reasonCode, "ADAPTER_ID_REQUIRED");
    assert.strictEqual(result.health, null);
    assert.strictEqual(db.table("deviceHealthChecks").length, 0, "unsupported must not be stored as offline");
  });

  await run("the default Reference Vendor shell does not claim unverified health support", async () => {
    const db = freshDb();
    const service = new DeviceIntegrationService(db, createDefaultDeviceAdapterRegistry());
    const result = await service.testConnection({ id: "dev-1", adapterId: "reference" });

    assert.strictEqual(result.status, "unknown");
    assert.strictEqual(result.reasonCode, "OPERATION_NOT_VERIFIED");
    assert.strictEqual(result.health, null);
    assert.strictEqual(db.table("deviceHealthChecks").length, 0);
  });

  if (process.exitCode) {
    console.error("\nSome device integration tests failed.");
  } else {
    console.log("\nAll device integration tests passed.");
  }
}

void main();
