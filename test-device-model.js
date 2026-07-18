const assert = require("assert");
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const storage = new Map();
function statusElement() {
  return {
    textContent: "",
    className: "",
    dataset: {}
  };
}
const elements = {
  deviceType: { value: "NVR" },
  deviceChannels: {
    value: "16",
    disabled: false,
    dataset: {},
    title: "",
    removeAttribute(name) {
      delete this[name];
    }
  },
  ptzStatus: statusElement(),
  monitorDrawerStatus: statusElement(),
  statusMessage: statusElement()
};
const context = {
  __VMS_TEST_MODE__: true,
  console,
  localStorage: {
    getItem(key) {
      return storage.has(key) ? storage.get(key) : null;
    },
    setItem(key, value) {
      storage.set(key, String(value));
    }
  },
  window: {
    location: { hash: "" },
    history: { replaceState() {}, pushState() {} }
  },
  document: {
    getElementById(id) {
      return elements[id] || null;
    },
    querySelector() {
      return null;
    },
    querySelectorAll() {
      return [];
    }
  }
};
context.globalThis = context;
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(__dirname, "app.js"), "utf8"), context, { filename: "app.js" });

const api = context.__VMS_TEST_API__;
assert(api, "app.js should expose the device model API in test mode");

function testAdministratorOnlyPrototypeContract() {
  assert.strictEqual(api.INITIAL_ROLE, "Administrator");
  assert(api.state.users.length >= 1, "the prototype should seed an administrator account");
  assert(api.state.users.every((user) => user.role === "Administrator"));
  assert(api.state.users.every((user) => JSON.stringify([...user.permissions]) === JSON.stringify([...api.FEATURE_CAPABILITIES])));

  const migrated = api.normalizeAdministratorUsers([{
    id: "usr-legacy",
    name: "Legacy Operator",
    username: "operator",
    role: "Operator",
    permissions: ["live"]
  }]);
  assert.strictEqual(migrated[0].role, "Administrator", "legacy prototype role records should migrate to the initial role");
  assert.deepStrictEqual([...migrated[0].permissions], [...api.FEATURE_CAPABILITIES]);

  const html = fs.readFileSync(path.join(__dirname, "index.html"), "utf8");
  const roleSelect = html.match(/<select id="userRoleInput"[\s\S]*?<\/select>/)?.[0] || "";
  assert.match(roleSelect, />Administrator</);
  assert.doesNotMatch(roleSelect, />Operator</);
  assert.doesNotMatch(roleSelect, />Supervisor</);
}

function testVendorNeutralDeviceFormContract() {
  const html = fs.readFileSync(path.join(__dirname, "index.html"), "utf8");
  const portInput = html.match(/<input id="devicePort"[^>]*>/)?.[0] || "";
  assert.match(portInput, /\brequired\b/);
  assert.doesNotMatch(portInput, /\bvalue="8000"/, "the common device form must not inject a Reference Vendor port");
  assert.match(html, /<h3>Add Device<\/h3>/);

  const deviceStatus = html.match(/<select id="deviceStatus"[\s\S]*?<\/select>/)?.[0] || "";
  const cameraStatus = html.match(/<select id="cameraStatusInput"[\s\S]*?<\/select>/)?.[0] || "";
  assert.match(deviceStatus, /value="unknown" selected/, "new devices must default to unknown");
  assert.match(cameraStatus, /value="unknown" selected/, "new cameras must default to unknown");
  assert.doesNotMatch(html, /All systems nominal/, "the initial banner must not claim unverified health");
  assert.doesNotMatch(
    html.match(/<p id="ptzStatus"[\s\S]*?<\/p>/)?.[0] || "",
    /queue commands/i,
    "unimplemented PTZ must not claim commands can be queued"
  );
}

function resetState({ devices, cameras = [], groups = [] }) {
  const selectedId = cameras[0]?.id || "";
  api.state.devices = devices;
  api.state.cameras = cameras;
  api.state.groups = groups;
  api.state.liveWorkspaces = [{
    id: "live-test",
    name: "Live Test",
    openIds: [],
    selectedId,
    gridDivision: 9,
    floating: false,
    layer: "front"
  }];
  api.state.playbackWorkspaces = [{
    id: "playback-test",
    name: "Playback Test",
    openIds: [],
    selectedId: "",
    gridDivision: 4,
    rangeStart: "2026-07-18 00:00:00",
    rangeEnd: "2026-07-18 23:59:59",
    floating: false,
    layer: "front"
  }];
  api.state.activeLiveWorkspaceId = "live-test";
  api.state.activePlaybackWorkspaceId = "playback-test";
  api.state.openIds = [];
  api.state.playbackOpenIds = [];
  api.state.gridDivision = 9;
  api.state.playbackGridDivision = 4;
  api.state.playbackSelectedId = "";
  api.state.playbackRangeStart = "2026-07-18 00:00:00";
  api.state.playbackRangeEnd = "2026-07-18 23:59:59";
  api.state.mapPositions = {};
  api.state.selectedId = selectedId;
  api.state.routeSelectedId = selectedId;
  api.state.mapSelectedId = selectedId;
  api.state.selectedGroupId = "";
}

function assignedGroup(deviceId) {
  return api.state.groups.find((group) => group.id === `grp-device-${deviceId}`);
}

function testLegacyMigrationAndRename() {
  resetState({
    devices: [{ id: "dev-legacy", name: "Old Recorder", type: "NVR", channels: 2, status: "online" }],
    cameras: [{
      id: "CAM-01",
      name: "Configured Camera",
      nvr: "Old Recorder",
      channel: 1,
      area: "Lobby",
      floor: "Ground",
      direction: "North",
      tags: ["configured"],
      related: [],
      previous: [],
      next: []
    }]
  });

  api.refreshCameraIndexes();
  assert.strictEqual(api.state.cameras[0].deviceId, "dev-legacy", "legacy name links should migrate to deviceId");
  api.syncDeviceChannels(api.state.devices[0]);
  api.refreshCameraIndexes();
  const idsBeforeRename = api.state.cameras.map((camera) => camera.id).sort();

  api.state.devices[0] = { ...api.state.devices[0], name: "Renamed Recorder" };
  api.syncDeviceChannels(api.state.devices[0]);
  api.refreshCameraIndexes();

  assert.deepStrictEqual(api.state.cameras.map((camera) => camera.id).sort(), idsBeforeRename, "rename must not duplicate channels");
  assert(api.state.cameras.every((camera) => camera.deviceId === "dev-legacy"), "rename must retain stable associations");
  assert(api.state.cameras.every((camera) => camera.nvr === "Renamed Recorder"), "legacy display name should follow rename");
  assert.strictEqual(assignedGroup("dev-legacy").name, "Renamed Recorder (Assigned)");
  assert.deepStrictEqual(
    [...assignedGroup("dev-legacy").cameraIds].sort(),
    [...idsBeforeRename].sort(),
    "system group should retain every device channel after rename"
  );
}

function testDirectIpCameraForcesOneChannel() {
  elements.deviceType.value = "IP Camera Direct";
  elements.deviceChannels.value = "16";
  api.applyDeviceTypeChannelRule();
  assert.strictEqual(elements.deviceChannels.value, 1, "type change should force the form field to one channel");
  assert.strictEqual(elements.deviceChannels.disabled, true, "direct IP channel field should be locked");

  resetState({
    devices: [{ id: "dev-ip", name: "Gate IP Camera", type: "IP Camera Direct", channels: 16, status: "online" }]
  });

  api.syncDeviceChannels(api.state.devices[0]);
  api.refreshCameraIndexes();

  assert.strictEqual(api.deviceChannelCount(api.state.devices[0]), 1);
  assert.strictEqual(api.state.devices[0].channels, 1, "direct IP device should be normalized to one channel");
  assert.strictEqual(api.state.cameras.length, 1, "direct IP device should create one camera placeholder");
  assert.strictEqual(api.state.cameras[0].deviceId, "dev-ip");
  assert.strictEqual(api.state.cameras[0].status, "unknown", "generated channels need direct evidence before claiming a state");
  assert.deepStrictEqual([...assignedGroup("dev-ip").cameraIds], [api.state.cameras[0].id]);
}

function testRecorderFailureDoesNotBecomeCameraEvidence() {
  resetState({
    devices: [{ id: "dev-offline", name: "Unreachable Recorder", type: "NVR", channels: 2, status: "offline" }]
  });

  api.syncDeviceChannels(api.state.devices[0]);
  api.refreshCameraIndexes();

  assert.strictEqual(api.state.cameras.length, 2);
  assert(
    api.state.cameras.every((camera) => camera.status === "unknown"),
    "recorder connectivity must not be copied into child camera health"
  );
}

function testUnimplementedPtzAndAuthorityHonesty() {
  resetState({
    devices: [{ id: "dev-ptz", name: "PTZ Recorder", type: "NVR", host: "192.0.2.10", port: 80, channels: 1, status: "unknown" }],
    cameras: [{
      id: "PTZ-01",
      name: "PTZ Camera",
      deviceId: "dev-ptz",
      nvr: "PTZ Recorder",
      channel: 1,
      area: "Test",
      floor: "Ground",
      direction: "North",
      status: "unknown",
      tags: [],
      related: [],
      previous: [],
      next: []
    }]
  });
  api.refreshCameraIndexes();

  const result = api.queuePtzAction("zoom-in");
  assert.strictEqual(result.status, "unavailable");
  assert.strictEqual(result.reasonCode, "OPERATION_NOT_IMPLEMENTED");
  assert.match(elements.ptzStatus.textContent, /unavailable/i);
  assert.doesNotMatch(elements.ptzStatus.textContent, /queued/i);

  assert.strictEqual(
    api.inventoryMutationAllowed({ apiAvailable: false, authorityState: "read_only" }),
    true,
    "file/local-only prototype mode must remain writable"
  );
  assert.strictEqual(
    api.inventoryMutationAllowed({ apiAvailable: true, authorityState: "connected" }),
    true
  );
  ["checking", "saving", "read_only", "save_failed"].forEach((authorityState) => {
    assert.strictEqual(
      api.inventoryMutationAllowed({ apiAvailable: true, authorityState }),
      false,
      `${authorityState} server-backed inventory must not accept another mutation`
    );
  });
}

function testChannelReductionPreservesConfiguredCamera() {
  resetState({
    devices: [{ id: "dev-reduce", name: "Recorder", type: "NVR", channels: 3, status: "online" }]
  });
  api.syncDeviceChannels(api.state.devices[0]);
  const configured = api.state.cameras.find((camera) => camera.channel === 3);
  Object.assign(configured, {
    name: "Configured Overflow Camera",
    area: "Warehouse",
    tags: ["configured"],
    managedPlaceholder: false
  });

  api.state.devices[0].channels = 1;
  const result = api.syncDeviceChannels(api.state.devices[0]);
  api.refreshCameraIndexes();

  assert.strictEqual(result.removedCount, 1, "unused generated channel should be removed");
  assert.strictEqual(result.detachedCount, 1, "configured out-of-range camera should be detached, not deleted");
  assert(api.state.cameras.some((camera) => camera.id === configured.id && camera.deviceId === ""));
  assert.strictEqual(assignedGroup("dev-reduce").cameraIds.length, 1, "system group should contain only valid channels");
}

function testInventoryBridgeKeepsCredentialsLocalAndAcceptsBackendAliases() {
  resetState({
    devices: [{
      id: "dev-bridge",
      name: "Bridge Camera",
      type: "IP Camera Direct",
      host: "192.168.60.10",
      port: 80,
      channels: 16,
      status: "online",
      username: "operator",
      password: "browser-only-secret"
    }]
  });

  const outgoing = api.createInventorySnapshot();
  assert.strictEqual(outgoing.devices[0].channels, 1);
  assert.strictEqual(JSON.stringify(outgoing).includes("browser-only-secret"), false, "passwords must not enter API payloads");

  api.applyInventorySnapshot({
    initialized: true,
    devices: [{
      id: "dev-bridge",
      name: "Renamed Bridge Camera",
      type: "IP Camera Direct",
      host: "192.168.60.10",
      port: 80,
      channelCount: 1,
      status: "online",
      username: "operator",
      password: ""
    }],
    cameras: [{
      id: "dev-bridge-CH01",
      displayName: "Bridge Camera View",
      deviceId: "dev-bridge",
      channelNumber: 1,
      status: "online",
      managedPlaceholder: true
    }],
    groups: []
  });

  assert.strictEqual(api.state.devices[0].password, "browser-only-secret", "remote hydration must preserve the local credential cache");
  assert.strictEqual(api.state.cameras[0].name, "Bridge Camera View");
  assert.strictEqual(api.state.cameras[0].nvr, "Renamed Bridge Camera");
  assert.deepStrictEqual(assignedGroup("dev-bridge").cameraIds, ["dev-bridge-CH01"]);
}

testAdministratorOnlyPrototypeContract();
testVendorNeutralDeviceFormContract();
testLegacyMigrationAndRename();
testDirectIpCameraForcesOneChannel();
testRecorderFailureDoesNotBecomeCameraEvidence();
testUnimplementedPtzAndAuthorityHonesty();
testChannelReductionPreservesConfiguredCamera();
testInventoryBridgeKeepsCredentialsLocalAndAcceptsBackendAliases();
console.log("Frontend device model regression tests passed.");
