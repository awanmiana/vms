const assert = require("assert");
const fs = require("fs");
const http = require("http");
const os = require("os");
const path = require("path");
const { FileDatabase } = require("./file-db");
const { createVmsServer } = require("./server");

function memoryDb() {
  const db = new FileDatabase(path.join(__dirname, "unused-api-test-db.json"));
  db.memoryOnly = true;
  return db;
}

function listen(server) {
  return new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", () => resolve(server));
  });
}

function close(server) {
  return new Promise((resolve, reject) => server.close((error) => (error ? reject(error) : resolve())));
}

function request(server, { method = "GET", pathname = "/", body } = {}) {
  return new Promise((resolve, reject) => {
    const address = server.address();
    const payload = body === undefined ? null : JSON.stringify(body);
    const req = http.request(
      {
        host: "127.0.0.1",
        port: address.port,
        method,
        path: pathname,
        headers: payload
          ? {
              "Content-Type": "application/json",
              "Content-Length": Buffer.byteLength(payload)
            }
          : undefined
      },
      (response) => {
        const chunks = [];
        response.on("data", (chunk) => chunks.push(chunk));
        response.on("end", () => {
          const text = Buffer.concat(chunks).toString("utf8");
          const isJson = String(response.headers["content-type"] || "").includes("application/json");
          resolve({
            status: response.statusCode,
            headers: response.headers,
            text,
            body: isJson && text ? JSON.parse(text) : null
          });
        });
      }
    );
    req.on("error", reject);
    if (payload) req.write(payload);
    req.end();
  });
}

async function withServer(options, fn) {
  const server = await listen(createVmsServer(options));
  try {
    return await fn(server);
  } finally {
    await close(server);
  }
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

const sampleInventory = {
  devices: [
    {
      id: "dev-nvr",
      name: "Gate NVR",
      type: "NVR",
      vendor: "Reference Vendor",
      host: "192.168.1.10",
      port: 8000,
      channels: 2,
      status: "online",
      username: "operator",
      password: "must-not-persist"
    },
    {
      id: "dev-dvr",
      name: "Floor DVR",
      type: "DVR",
      host: "192.168.1.11",
      port: 37777,
      channels: 1,
      status: "warning"
    },
    {
      id: "dev-ip",
      name: "Door IP Camera",
      type: "IP Camera Direct",
      host: "192.168.1.12",
      port: 80,
      channels: 16,
      status: "online"
    }
  ],
  cameras: [
    {
      id: "GATE-01",
      name: "Gate Entry",
      nvr: "Gate NVR",
      channel: 1,
      area: "Gate",
      status: "online",
      related: ["DOOR-IP", "missing-camera"]
    },
    {
      id: "FLOOR-01",
      displayName: "First Floor",
      deviceId: "dev-dvr",
      channelNumber: 1,
      status: "warning"
    },
    {
      id: "DOOR-IP",
      name: "Door Camera",
      deviceId: "dev-ip",
      channel: 1,
      status: "online",
      managedPlaceholder: true
    }
  ],
  groups: [
    {
      id: "grp-custom",
      name: "Custom Route",
      grid: 4,
      cameraIds: ["GATE-01", "DOOR-IP", "missing-camera"]
    },
    {
      id: "forged-system-group",
      name: "Client controlled system group",
      system: true,
      deviceId: "dev-nvr",
      cameraIds: ["DOOR-IP"]
    }
  ]
};

async function main() {
  await run("unmarked diagnostic data is not exposed as initialized UI inventory", async () => {
    const db = memoryDb();
    db.store.devices.push({ id: "dev-check", name: "Check NVR", type: "NVR" });
    db.store.cameras.push({ id: "CHECK-01", deviceId: "dev-check" });

    await withServer({ db }, async (server) => {
      const response = await request(server, { pathname: "/api/inventory" });
      assert.strictEqual(response.status, 200);
      assert.deepStrictEqual(response.body, {
        initialized: false,
        devices: [],
        cameras: [],
        groups: []
      });
    });
  });

  await run("PUT inventory normalizes state and derives one protected group per device", async () => {
    const db = memoryDb();

    await withServer({ db }, async (server) => {
      const saved = await request(server, { method: "PUT", pathname: "/api/inventory", body: sampleInventory });
      assert.strictEqual(saved.status, 200, saved.text);
      assert.strictEqual(saved.body.initialized, true);
      assert.strictEqual(saved.body.devices.length, 3);
      assert.deepStrictEqual(saved.body.devices.map((device) => device.port), [8000, 37777, 80]);
      assert.strictEqual(saved.body.devices[0].password, "");
      assert.strictEqual(
        saved.body.devices.find((device) => device.id === "dev-ip").channels,
        1,
        "direct IP cameras must be normalized to one channel"
      );
      assert.strictEqual(JSON.stringify(db.store).includes("must-not-persist"), false);

      assert.strictEqual(saved.body.cameras[0].deviceId, "dev-nvr");
      assert.deepStrictEqual(saved.body.cameras[0].related, ["DOOR-IP"]);
      assert.strictEqual(saved.body.cameras[1].name, "First Floor");
      assert.strictEqual(saved.body.cameras[1].nvr, "Floor DVR");
      assert.strictEqual(saved.body.cameras[2].managedPlaceholder, true);

      assert.strictEqual(saved.body.groups.length, 4);
      assert.strictEqual(saved.body.groups.some((group) => group.id === "forged-system-group"), false);
      assert.deepStrictEqual(
        saved.body.groups.find((group) => group.id === "grp-custom").cameraIds,
        ["GATE-01", "DOOR-IP"]
      );

      const expectedMembers = {
        "grp-device-dev-nvr": ["GATE-01"],
        "grp-device-dev-dvr": ["FLOOR-01"],
        "grp-device-dev-ip": ["DOOR-IP"]
      };
      Object.entries(expectedMembers).forEach(([groupId, cameraIds]) => {
        const group = saved.body.groups.find((item) => item.id === groupId);
        assert.ok(group, `${groupId} should exist`);
        assert.strictEqual(group.system, true);
        assert.deepStrictEqual(group.cameraIds, cameraIds);
      });

      const loaded = await request(server, { pathname: "/api/inventory" });
      assert.deepStrictEqual(loaded.body, saved.body);
      assert.strictEqual(db.store.inventoryState.initialized, true);
    });
  });

  await run("a rejected PUT leaves the previous complete inventory unchanged", async () => {
    const db = memoryDb();

    await withServer({ db }, async (server) => {
      const first = await request(server, { method: "PUT", pathname: "/api/inventory", body: sampleInventory });
      assert.strictEqual(first.status, 200);

      const invalid = {
        devices: [sampleInventory.devices[0], { ...sampleInventory.devices[1], id: "dev-nvr" }],
        cameras: [],
        groups: []
      };
      const rejected = await request(server, { method: "PUT", pathname: "/api/inventory", body: invalid });
      assert.strictEqual(rejected.status, 400);
      assert.match(rejected.body.message, /Duplicate device id/);

      const after = await request(server, { pathname: "/api/inventory" });
      assert.deepStrictEqual(after.body, first.body);
    });
  });

  await run("a persistence fallback rejects durable inventory writes without changing cached state", async () => {
    const db = memoryDb();

    await withServer({ db }, async (server) => {
      const first = await request(server, { method: "PUT", pathname: "/api/inventory", body: sampleInventory });
      assert.strictEqual(first.status, 200);

      const fallbackError = new Error("The inventory file is temporarily unavailable.");
      fallbackError.code = "ENOENT";
      db.markPersistenceUnavailable(fallbackError);

      const changed = {
        devices: first.body.devices.map((device) =>
          device.id === "dev-nvr" ? { ...device, name: "Unsaved Rename" } : device
        ),
        cameras: first.body.cameras,
        groups: first.body.groups
      };
      const rejected = await request(server, {
        method: "PUT",
        pathname: "/api/inventory",
        body: changed
      });

      assert.strictEqual(rejected.status, 503);
      assert.strictEqual(rejected.body.durable, false);
      assert.match(rejected.body.message, /not saved/);

      const after = await request(server, { pathname: "/api/inventory" });
      assert.deepStrictEqual(after.body, first.body);
    });
  });

  await run("inventory requires an explicit device port instead of assuming a vendor default", async () => {
    const db = memoryDb();
    const deviceWithoutPort = { ...sampleInventory.devices[0] };
    delete deviceWithoutPort.port;
    const payload = {
      devices: [deviceWithoutPort],
      cameras: [],
      groups: []
    };

    await withServer({ db }, async (server) => {
      const rejected = await request(server, { method: "PUT", pathname: "/api/inventory", body: payload });
      assert.strictEqual(rejected.status, 400);
      assert.match(rejected.body.message, /port must be an integer from 1 to 65535/);
      assert.strictEqual(db.store.inventoryState?.initialized, undefined);
    });
  });

  await run("deleting a device unassigns its retained cameras and removes its system group", async () => {
    const db = memoryDb();

    await withServer({ db }, async (server) => {
      const first = await request(server, { method: "PUT", pathname: "/api/inventory", body: sampleInventory });
      assert.strictEqual(first.status, 200);

      const withoutNvr = {
        devices: first.body.devices.filter((device) => device.id !== "dev-nvr"),
        cameras: first.body.cameras,
        groups: first.body.groups
      };
      const saved = await request(server, { method: "PUT", pathname: "/api/inventory", body: withoutNvr });
      assert.strictEqual(saved.status, 200, saved.text);
      const retainedCamera = saved.body.cameras.find((camera) => camera.id === "GATE-01");
      assert.strictEqual(retainedCamera.deviceId, "");
      assert.strictEqual(retainedCamera.nvr, "Gate NVR");
      assert.strictEqual(saved.body.groups.some((group) => group.id === "grp-device-dev-nvr"), false);
    });
  });

  await run("initialization marker and normalized inventory survive a server restart", async () => {
    const tempDirectory = fs.mkdtempSync(path.join(os.tmpdir(), "vms-inventory-api-"));
    const databasePath = path.join(tempDirectory, "inventory.json");
    try {
      await withServer({ databasePath }, async (server) => {
        const response = await request(server, {
          method: "PUT",
          pathname: "/api/inventory",
          body: sampleInventory
        });
        assert.strictEqual(response.status, 200, response.text);
      });

      const stored = JSON.parse(fs.readFileSync(databasePath, "utf8"));
      assert.strictEqual(stored.inventoryState.initialized, true);
      assert.strictEqual(stored.inventoryState.version, 1);

      await withServer({ databasePath }, async (server) => {
        const response = await request(server, { pathname: "/api/inventory" });
        assert.strictEqual(response.body.initialized, true);
        assert.strictEqual(response.body.devices.length, 3);
      });
    } finally {
      if (fs.existsSync(databasePath)) fs.unlinkSync(databasePath);
      fs.rmdirSync(tempDirectory);
    }
  });

  await run("server serves the frontend but blocks private backend files", async () => {
    await withServer({ db: memoryDb(), staticRoot: path.join(__dirname, "..") }, async (server) => {
      const index = await request(server, { pathname: "/" });
      assert.strictEqual(index.status, 200);
      assert.match(index.headers["content-type"], /text\/html/);
      assert.match(index.text, /VMS Operator Console/);

      const script = await request(server, { pathname: "/app.js" });
      assert.strictEqual(script.status, 200);
      assert.match(script.headers["content-type"], /javascript/);

      const privateFile = await request(server, { pathname: "/backend/vms-dev-db.json" });
      assert.strictEqual(privateFile.status, 404);
    });
  });

  await run("existing compliance API stubs remain reachable", async () => {
    await withServer({ db: memoryDb() }, async (server) => {
      const response = await request(server, { method: "POST", pathname: "/api/compliance-logs" });
      assert.strictEqual(response.status, 501);
      assert.match(response.body.message, /future external tools/);
    });
  });

  if (process.exitCode) {
    console.error("\nSome API server tests failed.");
  } else {
    console.log("\nAll API server tests passed.");
  }
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
