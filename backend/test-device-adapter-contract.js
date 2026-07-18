const assert = require("assert");
const {
  ADAPTER_CONTRACT_VERSION,
  DeviceAdapterContractError,
  DeviceAdapterOperationError,
  DeviceAdapterRegistry,
  createDefaultDeviceAdapterRegistry,
  createDeviceAdapter
} = require("./device-adapters");

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

function fakeAdapter(id, operations = {}, aliases = []) {
  return createDeviceAdapter({
    manifest: {
      contractVersion: ADAPTER_CONTRACT_VERSION,
      id,
      displayName: `${id} test adapter`,
      aliases,
      implementationStatus: "test-only"
    },
    operations
  });
}

async function main() {
  await run("Reference Vendor is the first and only default adapter identity", async () => {
    const registry = createDefaultDeviceAdapterRegistry();
    const manifests = registry.list();
    assert.deepStrictEqual(manifests.map((manifest) => manifest.id), ["reference"]);
    assert.strictEqual(manifests[0].implementationStatus, "identity-only");
    assert.strictEqual(Object.keys(registry.resolve("reference").adapter.operations).length, 0);
  });

  await run("adapter ids and declared aliases resolve case-insensitively", async () => {
    const registry = createDefaultDeviceAdapterRegistry();
    assert.strictEqual(registry.resolve("  REFERENCE  ").manifest.id, "reference");
  });

  await run("missing and unknown adapters never fall back to Reference Vendor or open interoperability", async () => {
    const registry = createDefaultDeviceAdapterRegistry();
    assert.strictEqual(registry.resolve("").reasonCode, "ADAPTER_ID_REQUIRED");
    assert.strictEqual(registry.resolve("interoperability").reasonCode, "ADAPTER_NOT_REGISTERED");
    assert.strictEqual(registry.resolve("unknown-vendor").reasonCode, "ADAPTER_NOT_REGISTERED");

    const outcome = await registry.execute("interoperability", "device.health", { type: "NVR" });
    assert.strictEqual(outcome.status, "unavailable");
    assert.strictEqual(outcome.reasonCode, "ADAPTER_NOT_REGISTERED");
  });

  await run("registered but unverified Reference Vendor operations remain unknown", async () => {
    const registry = createDefaultDeviceAdapterRegistry();
    const outcome = await registry.execute("reference", "device.health", {});
    assert.strictEqual(outcome.status, "unknown");
    assert.strictEqual(outcome.reasonCode, "OPERATION_NOT_VERIFIED");
  });

  await run("explicitly unsupported operations remain distinct from unknown operations", async () => {
    const registry = new DeviceAdapterRegistry();
    registry.register(createDeviceAdapter({
      manifest: {
        contractVersion: ADAPTER_CONTRACT_VERSION,
        id: "limited",
        displayName: "Limited adapter"
      },
      unsupportedOperations: ["camera.ptz"]
    }));

    const outcome = await registry.execute("limited", "camera.ptz", {});
    assert.strictEqual(outcome.status, "unsupported");
    assert.strictEqual(outcome.reasonCode, "OPERATION_UNSUPPORTED");
  });

  await run("a post-send adapter failure can report an explicitly unknown outcome", async () => {
    const registry = new DeviceAdapterRegistry().register(fakeAdapter("uncertain", {
      "camera.ptz": async () => {
        throw new DeviceAdapterOperationError(
          "DEVICE_RESPONSE_LOST",
          "The command was sent but its response was lost.",
          { outcomeUnknown: true }
        );
      },
      "device.health": async () => {
        throw new DeviceAdapterOperationError("CONNECTION_REFUSED", "The connection was refused.");
      }
    }));

    const uncertain = await registry.execute("uncertain", "camera.ptz", {});
    assert.strictEqual(uncertain.status, "unknown");
    assert.strictEqual(uncertain.reasonCode, "DEVICE_RESPONSE_LOST");
    assert.strictEqual(uncertain.outcomeUnknown, true);

    const failed = await registry.execute("uncertain", "device.health", {});
    assert.strictEqual(failed.status, "failed");
    assert.strictEqual(failed.reasonCode, "CONNECTION_REFUSED");
    assert.strictEqual(failed.outcomeUnknown, undefined);
  });

  await run("an arbitrary second adapter executes without core logic changes", async () => {
    const registry = createDefaultDeviceAdapterRegistry();
    registry.register(fakeAdapter("acme", {
      "device.health": async ({ device }) => ({ ok: true, model: device.model })
    }, ["acme-cameras"]));

    const outcome = await registry.execute("ACME CAMERAS", "device.health", {
      device: { model: "A-100" }
    });
    assert.strictEqual(outcome.status, "succeeded");
    assert.strictEqual(outcome.adapterId, "acme");
    assert.strictEqual(outcome.data.model, "A-100");
  });

  await run("duplicate ids, duplicate aliases and invalid manifests are rejected", async () => {
    const registry = new DeviceAdapterRegistry();
    registry.register(fakeAdapter("first", {}, ["shared"]));
    assert.throws(
      () => registry.register(fakeAdapter("first")),
      (error) => error instanceof DeviceAdapterContractError && error.code === "DUPLICATE_ADAPTER_ID"
    );
    assert.throws(
      () => registry.register(fakeAdapter("second", {}, ["shared"])),
      (error) => error instanceof DeviceAdapterContractError && error.code === "DUPLICATE_ADAPTER_ALIAS"
    );
    assert.throws(
      () => createDeviceAdapter({ manifest: { contractVersion: 999, id: "bad", displayName: "Bad" } }),
      (error) => error instanceof DeviceAdapterContractError && error.code === "UNSUPPORTED_ADAPTER_CONTRACT"
    );
  });

  if (process.exitCode) {
    console.error("\nSome device adapter contract tests failed.");
  } else {
    console.log("\nAll device adapter contract tests passed.");
  }
}

void main();
