const fs = require("fs");
const path = require("path");
const { formatDateTime } = require("./datetime");

const INVENTORY_VERSION = 1;
const DEVICE_TYPES = new Set(["NVR", "DVR", "Hybrid DVR", "IP Camera Direct"]);
const DEVICE_STATUSES = new Set(["online", "offline", "warning", "unknown"]);

class InventoryValidationError extends Error {
  constructor(message) {
    super(message);
    this.name = "InventoryValidationError";
  }
}

function notImplemented(route) {
  return {
    status: 501,
    body: {
      error: "Not Implemented",
      route,
      message: "Reserved VMS integration endpoint for future external tools."
    }
  };
}

const routes = {
  "POST /api/compliance-logs": () => notImplemented("POST /api/compliance-logs"),
  "POST /api/tickets": () => notImplemented("POST /api/tickets"),
  "GET /api/entities": () => notImplemented("GET /api/entities"),
  "GET /api/locations": () => notImplemented("GET /api/locations"),
  "GET /api/compliance-types": () => notImplemented("GET /api/compliance-types")
};

function handleStubRoute(method, pathname) {
  const key = `${String(method || "").toUpperCase()} ${pathname}`;
  const handler = routes[key];
  if (!handler) {
    return {
      status: 404,
      body: {
        error: "Not Found",
        route: key
      }
    };
  }
  return handler();
}

function cleanText(value, { fallback = "", max = 256, required = false, label = "value" } = {}) {
  const text = value === undefined || value === null ? fallback : String(value);
  const cleaned = text.replace(/[\u0000-\u0008\u000B\u000C\u000E-\u001F\u007F]/g, "").trim();
  if (required && !cleaned) {
    throw new InventoryValidationError(`${label} is required.`);
  }
  if (cleaned.length > max) {
    throw new InventoryValidationError(`${label} must be ${max} characters or fewer.`);
  }
  return cleaned;
}

function cleanId(value, label) {
  const id = cleanText(value, { required: true, max: 128, label });
  if (!/^[A-Za-z0-9][A-Za-z0-9._:-]*$/.test(id)) {
    throw new InventoryValidationError(`${label} contains unsupported characters.`);
  }
  return id;
}

function integerInRange(value, fallback, min, max, label) {
  const number = value === undefined || value === null || value === "" ? fallback : Number(value);
  if (!Number.isInteger(number) || number < min || number > max) {
    throw new InventoryValidationError(`${label} must be an integer from ${min} to ${max}.`);
  }
  return number;
}

function uniqueStrings(value, { maxItems = 128, maxLength = 128 } = {}) {
  if (!Array.isArray(value)) return [];
  const cleaned = value
    .slice(0, maxItems)
    .map((item) => cleanText(item, { max: maxLength }))
    .filter(Boolean);
  return [...new Set(cleaned)];
}

function requireArray(payload, key) {
  if (!Array.isArray(payload[key])) {
    throw new InventoryValidationError(`${key} must be an array.`);
  }
  return payload[key];
}

function rejectDuplicate(seen, value, label) {
  if (seen.has(value)) {
    throw new InventoryValidationError(`Duplicate ${label}: ${value}.`);
  }
  seen.add(value);
}

function normalizeDevice(raw, index, seenIds, seenNames) {
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) {
    throw new InventoryValidationError(`devices[${index}] must be an object.`);
  }

  const id = cleanId(raw.id, `devices[${index}].id`);
  const name = cleanText(raw.name, { required: true, max: 160, label: `devices[${index}].name` });
  rejectDuplicate(seenIds, id, "device id");
  rejectDuplicate(seenNames, name.toLowerCase(), "device name");

  const type = cleanText(raw.type, { fallback: "NVR", max: 32, label: `devices[${index}].type` });
  if (!DEVICE_TYPES.has(type)) {
    throw new InventoryValidationError(`devices[${index}].type is not supported.`);
  }

  const status = cleanText(raw.status, { fallback: "unknown", max: 16 }).toLowerCase();
  if (!DEVICE_STATUSES.has(status)) {
    throw new InventoryValidationError(`devices[${index}].status is not supported.`);
  }

  const requestedChannels = integerInRange(
    raw.channels ?? raw.channelCount,
    1,
    1,
    256,
    `devices[${index}].channels`
  );
  const channels = type === "IP Camera Direct" ? 1 : requestedChannels;

  return {
    id,
    name,
    type,
    vendor: cleanText(raw.vendor, { max: 128 }),
    host: cleanText(raw.host, { required: true, max: 255, label: `devices[${index}].host` }),
    port: integerInRange(raw.port, Number.NaN, 1, 65535, `devices[${index}].port`),
    channels,
    channelCount: channels,
    status,
    username: cleanText(raw.username, { max: 128 }),
    // Passwords deliberately never cross this persistence boundary.
    password: "",
    notes: cleanText(raw.notes, { max: 4000 }),
    maxConcurrentMainstream: integerInRange(
      raw.maxConcurrentMainstream,
      4,
      1,
      256,
      `devices[${index}].maxConcurrentMainstream`
    ),
    maxConcurrentSubstream: integerInRange(
      raw.maxConcurrentSubstream,
      32,
      1,
      1024,
      `devices[${index}].maxConcurrentSubstream`
    )
  };
}

function normalizeCamera(raw, index, devicesById, devicesByName, seenIds, seenDeviceChannels) {
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) {
    throw new InventoryValidationError(`cameras[${index}] must be an object.`);
  }

  const id = cleanId(raw.id, `cameras[${index}].id`);
  rejectDuplicate(seenIds, id, "camera id");

  const suppliedDeviceId = cleanText(raw.deviceId, { max: 128 });
  const suppliedDeviceName = cleanText(raw.nvr, { max: 160 });
  // A deleted device may leave manual/sample cameras behind. Prefer the stable id,
  // fall back to the legacy recorder name, and otherwise keep the camera unassigned.
  const device = devicesById.get(suppliedDeviceId)
    || devicesByName.get(suppliedDeviceName.toLowerCase());

  const channel = integerInRange(
    raw.channel ?? raw.channelNumber,
    1,
    1,
    4096,
    `cameras[${index}].channel`
  );
  if (device) {
    if (channel > device.channels) {
      throw new InventoryValidationError(
        `cameras[${index}].channel exceeds the configured channel count for ${device.name}.`
      );
    }
    rejectDuplicate(seenDeviceChannels, `${device.id}:${channel}`, "device camera channel");
  }

  const name = cleanText(raw.name ?? raw.displayName, {
    required: true,
    max: 160,
    label: `cameras[${index}].name`
  });

  const status = cleanText(raw.status, { fallback: "unknown", max: 16 }).toLowerCase();
  if (!DEVICE_STATUSES.has(status)) {
    throw new InventoryValidationError(`cameras[${index}].status is not supported.`);
  }

  const tags = uniqueStrings(raw.tags);
  const area = cleanText(raw.area, { fallback: "Unassigned", max: 160 });
  const floor = cleanText(raw.floor, { fallback: "Unknown", max: 128 });
  const direction = cleanText(raw.direction, { fallback: "Direction not set", max: 512 });
  const managedPlaceholder =
    raw.managedPlaceholder === true ||
    raw.deviceSyncManaged === true ||
    (
      raw.discovered === true &&
      tags.includes("unmapped") &&
      area === "Unassigned" &&
      floor === "Unknown" &&
      direction === "Direction not set"
    );

  return {
    id,
    deviceId: device?.id || "",
    name,
    displayName: name,
    nvr: device?.name || suppliedDeviceName || "Unassigned",
    channel,
    channelNumber: channel,
    stream: cleanText(raw.stream, { max: 2048 }),
    area,
    floor,
    direction,
    status,
    tags,
    related: uniqueStrings(raw.related),
    next: uniqueStrings(raw.next),
    previous: uniqueStrings(raw.previous),
    discovered: Boolean(raw.discovered),
    managedPlaceholder,
    deviceSyncManaged: managedPlaceholder
  };
}

function normalizeCustomGroup(raw, index, cameraIds, reservedSystemIds, seenIds) {
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) {
    throw new InventoryValidationError(`groups[${index}] must be an object.`);
  }

  // System groups are derived below. Client copies are never trusted as source data.
  if (raw.system === true || raw.isSystem === true || raw.deviceId) return null;

  const id = cleanId(raw.id, `groups[${index}].id`);
  if (reservedSystemIds.has(id)) return null;
  rejectDuplicate(seenIds, id, "group id");

  return {
    id,
    name: cleanText(raw.name, { required: true, max: 160, label: `groups[${index}].name` }),
    purpose: cleanText(raw.purpose, { max: 512 }),
    grid: integerInRange(raw.grid ?? raw.preferredGrid, 4, 1, 64, `groups[${index}].grid`),
    cameraIds: uniqueStrings(raw.cameraIds, { maxItems: 4096 }).filter((cameraId) => cameraIds.has(cameraId)),
    notes: cleanText(raw.notes, { max: 4000 }),
    system: false
  };
}

function systemGroupFor(device, cameras) {
  const cameraIds = cameras
    .filter((camera) => camera.deviceId === device.id)
    .sort((a, b) => a.channel - b.channel || a.id.localeCompare(b.id))
    .map((camera) => camera.id);

  return {
    id: `grp-device-${device.id}`,
    name: `${device.name} (Assigned)`,
    purpose: "Auto-assigned device channels",
    grid: 4,
    cameraIds,
    notes: "Created automatically from this device's channels.",
    system: true,
    isSystem: true,
    deviceId: device.id
  };
}

function normalizeInventory(payload) {
  if (!payload || typeof payload !== "object" || Array.isArray(payload)) {
    throw new InventoryValidationError("Request body must be a JSON object.");
  }

  const deviceIds = new Set();
  const deviceNames = new Set();
  const devices = requireArray(payload, "devices").map((device, index) =>
    normalizeDevice(device, index, deviceIds, deviceNames)
  );
  const devicesById = new Map(devices.map((device) => [device.id, device]));
  const devicesByName = new Map(devices.map((device) => [device.name.toLowerCase(), device]));

  const cameraIds = new Set();
  const deviceChannels = new Set();
  const cameras = requireArray(payload, "cameras").map((camera, index) =>
    normalizeCamera(camera, index, devicesById, devicesByName, cameraIds, deviceChannels)
  );

  // Remove route links that point outside this inventory.
  cameras.forEach((camera) => {
    camera.related = camera.related.filter((id) => cameraIds.has(id));
    camera.next = camera.next.filter((id) => cameraIds.has(id));
    camera.previous = camera.previous.filter((id) => cameraIds.has(id));
  });

  const reservedSystemIds = new Set(devices.map((device) => `grp-device-${device.id}`));
  const customGroupIds = new Set();
  const customGroups = requireArray(payload, "groups")
    .map((group, index) => normalizeCustomGroup(group, index, cameraIds, reservedSystemIds, customGroupIds))
    .filter(Boolean);
  const systemGroups = devices.map((device) => systemGroupFor(device, cameras));

  return { devices, cameras, groups: [...customGroups, ...systemGroups] };
}

function isInventoryInitialized(db) {
  return db?.store?.inventoryState?.initialized === true;
}

function emptyInventorySnapshot() {
  return { initialized: false, devices: [], cameras: [], groups: [] };
}

function readInventory(db) {
  if (!isInventoryInitialized(db)) return emptyInventorySnapshot();
  const inventory = normalizeInventory({
    devices: db.store.devices,
    cameras: db.store.cameras,
    groups: db.store.groups
  });
  return { initialized: true, ...inventory };
}

function persistenceUnavailableError() {
  const error = new Error("Durable inventory persistence is unavailable; the inventory was not saved.");
  error.code = "PERSISTENCE_UNAVAILABLE";
  return error;
}

function writeStoreAtomically(db, store) {
  if (db.persistenceFallback) {
    throw persistenceUnavailableError();
  }
  if (db.memoryOnly || !db.filePath) return;

  const targetPath = path.resolve(db.filePath);
  const directory = path.dirname(targetPath);
  const tempPath = path.join(
    directory,
    `.${path.basename(targetPath)}.${process.pid}.${Date.now()}.tmp`
  );
  fs.mkdirSync(directory, { recursive: true });

  try {
    const handle = fs.openSync(tempPath, "wx");
    try {
      fs.writeFileSync(handle, `${JSON.stringify(store, null, 2)}\n`, "utf8");
      fs.fsyncSync(handle);
    } finally {
      fs.closeSync(handle);
    }
    fs.renameSync(tempPath, targetPath);
  } catch (error) {
    try {
      if (fs.existsSync(tempPath)) fs.unlinkSync(tempPath);
    } catch {
      // Preserve the original persistence error.
    }
    if (typeof db.markPersistenceUnavailable === "function") {
      db.markPersistenceUnavailable(error);
    }
    const unavailable = persistenceUnavailableError();
    unavailable.cause = error;
    throw unavailable;
  }
}

function persistInventory(db, payload) {
  const inventory = normalizeInventory(payload);
  const previousStore = db.store;
  const nextStore = {
    ...previousStore,
    devices: inventory.devices,
    cameras: inventory.cameras,
    groups: inventory.groups,
    inventoryState: {
      initialized: true,
      version: INVENTORY_VERSION,
      updatedAt: formatDateTime()
    }
  };

  try {
    writeStoreAtomically(db, nextStore);
    db.store = nextStore;
  } catch (error) {
    db.store = previousStore;
    throw error;
  }

  return { initialized: true, ...inventory };
}

function handleInventoryRoute(db, method, pathname, body) {
  if (pathname !== "/api/inventory") return null;
  const normalizedMethod = String(method || "").toUpperCase();

  try {
    if (normalizedMethod === "GET") {
      return { status: 200, body: readInventory(db) };
    }
    if (normalizedMethod === "PUT") {
      return { status: 200, body: persistInventory(db, body) };
    }
    return {
      status: 405,
      headers: { Allow: "GET, PUT" },
      body: { error: "Method Not Allowed", route: `${normalizedMethod} ${pathname}` }
    };
  } catch (error) {
    if (error instanceof InventoryValidationError) {
      return { status: 400, body: { error: "Invalid inventory", message: error.message } };
    }
    if (error.code === "PERSISTENCE_UNAVAILABLE") {
      return {
        status: 503,
        body: {
          error: "Inventory persistence unavailable",
          message: error.message,
          durable: false
        }
      };
    }
    return {
      status: 500,
      body: { error: "Inventory persistence failed", message: error.message }
    };
  }
}

module.exports = {
  DEVICE_TYPES,
  INVENTORY_VERSION,
  InventoryValidationError,
  handleInventoryRoute,
  handleStubRoute,
  isInventoryInitialized,
  normalizeInventory,
  persistInventory,
  readInventory,
  routes
};
