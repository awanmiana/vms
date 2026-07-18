const crypto = require("crypto");
const { estimateBitrateKbps, resolveTier } = require("./media-policy");

const SUPPORTED_DEVICE_TYPES = Object.freeze(["NVR", "DVR", "Hybrid DVR", "IP Camera Direct"]);
const supportedDeviceTypeSet = new Set(SUPPORTED_DEVICE_TYPES);

function id(prefix) {
  return `${prefix}-${crypto.randomUUID()}`;
}

function slug(value) {
  return String(value)
    .toUpperCase()
    .replace(/[^A-Z0-9]+/g, "-")
    .replace(/^-|-$/g, "")
    .slice(0, 24);
}

function normalizeChannelCount(value) {
  const parsed = Number(value);
  if (!Number.isFinite(parsed)) return 1;
  return Math.max(1, Math.min(256, Math.floor(parsed)));
}

function normalizeDeviceType(value) {
  const type = String(value || "NVR").trim();
  if (!supportedDeviceTypeSet.has(type)) {
    throw new Error(`Unsupported device type: ${type || "(empty)"}`);
  }
  return type;
}

function normalizeDeviceChannelCount(type, value) {
  return type === "IP Camera Direct" ? 1 : normalizeChannelCount(value);
}

function channelDisplayName(device, channelNumber) {
  return `${device.name} CH-${String(channelNumber).padStart(2, "0")}`;
}

function channelCameraId(device, channelNumber) {
  const deviceKey = String(device.id || "").trim() || slug(device.name || device.host || "DEVICE");
  return `${deviceKey}-CH${String(channelNumber).padStart(2, "0")}`;
}

function legacyChannelCameraId(deviceName, channelNumber) {
  return `${slug(deviceName || "DEVICE")}-CH${String(channelNumber).padStart(2, "0")}`;
}

function deviceGroupId(deviceOrId) {
  const deviceId = typeof deviceOrId === "string" ? deviceOrId : deviceOrId?.id;
  if (!deviceId) throw new Error("A device id is required to create its system group.");
  return `grp-device-${deviceId}`;
}

function ensureDeviceGroupRow(db, device, cameras = db.table("cameras").filter((camera) => camera.deviceId === device.id)) {
  const groupId = deviceGroupId(device);
  const existing = db.table("groups").find((group) => group.id === groupId);
  const cameraIds = cameras
    .slice()
    .sort((left, right) => Number(left.channelNumber) - Number(right.channelNumber))
    .map((camera) => camera.id);
  const group = {
    ...(existing || {}),
    id: groupId,
    name: `${device.name} (Assigned)`,
    purpose: existing?.purpose || "Auto-assigned device channels",
    grid: existing?.grid || 4,
    cameraIds,
    notes: existing?.notes || "Created automatically from this device's channels.",
    system: true,
    isSystem: true,
    deviceId: device.id
  };
  db.upsert("groups", group);
  return db.table("groups").find((candidate) => candidate.id === groupId);
}

class DeviceService {
  constructor(db) {
    this.db = db;
  }

  list() {
    return this.db.table("devices");
  }

  save(device) {
    const type = normalizeDeviceType(device.type);
    const port = Number(device.port);
    if (!Number.isInteger(port) || port < 1 || port > 65535) {
      throw new Error("Device port must be an explicit integer from 1 to 65535.");
    }
    const row = {
      id: device.id || id("dev"),
      name: device.name,
      type,
      vendor: device.vendor || "",
      host: device.host,
      port,
      channelCount: normalizeDeviceChannelCount(type, device.channelCount || device.channels || 1),
      status: device.status || "unknown",
      maxConcurrentMainstream: Number(device.maxConcurrentMainstream || 4),
      maxConcurrentSubstream: Number(device.maxConcurrentSubstream || 32),
      notes: device.notes || ""
    };

    this.db.upsert("devices", row);
    ensureDeviceGroupRow(this.db, row);
    return row;
  }
}

class CameraService {
  constructor(db) {
    this.db = db;
  }

  list() {
    return this.db.table("cameras");
  }

  syncDeviceChannels(device) {
    if (!device?.id) throw new Error("A saved device with an id is required to sync channels.");

    const type = normalizeDeviceType(device.type);
    const channelCount = normalizeDeviceChannelCount(type, device.channelCount || device.channels || 1);
    const deviceCameras = this.db.table("cameras").filter((camera) => camera.deviceId === device.id);
    const existingByChannel = new Map();
    const staleCameraIds = new Set();

    deviceCameras.forEach((camera) => {
      const channelNumber = Number(camera.channelNumber);
      if (!Number.isInteger(channelNumber) || channelNumber < 1 || channelNumber > channelCount || existingByChannel.has(channelNumber)) {
        staleCameraIds.add(camera.id);
        return;
      }
      existingByChannel.set(channelNumber, camera);
    });

    for (let channelNumber = 1; channelNumber <= channelCount; channelNumber += 1) {
      const existing = existingByChannel.get(channelNumber);
      const generatedDisplayName = channelDisplayName(device, channelNumber);

      if (existing) {
        const displayNameWasManaged =
          !existing.displayName ||
          existing.displayName === existing.syncedDisplayName ||
          this.isLegacyGeneratedDisplayName(existing, channelNumber);

        this.db.upsert("cameras", {
          ...existing,
          deviceId: device.id,
          channelNumber,
          displayName: displayNameWasManaged ? generatedDisplayName : existing.displayName,
          syncedDisplayName: generatedDisplayName,
          syncedDeviceName: device.name,
          deviceSyncManaged: true
        });
        this.createDefaultProfiles(existing.id);
        continue;
      }

      const camera = {
        id: channelCameraId(device, channelNumber),
        deviceId: device.id,
        channelNumber,
        displayName: generatedDisplayName,
        syncedDisplayName: generatedDisplayName,
        syncedDeviceName: device.name,
        deviceSyncManaged: true,
        area: "Unassigned",
        floor: "Unknown",
        direction: "Direction not set",
        // A parent recorder status is not direct evidence about an individual
        // channel. Generated children remain unknown until checked themselves.
        status: "unknown",
        discovered: true,
        tags: ["unmapped"]
      };
      this.db.upsert("cameras", camera);
      this.createDefaultProfiles(camera.id);
    }

    staleCameraIds.forEach((cameraId) => this.removeSyncedCamera(cameraId));

    const syncedCameras = this.list()
      .filter((camera) => camera.deviceId === device.id)
      .sort((left, right) => Number(left.channelNumber) - Number(right.channelNumber));
    this.ensureDeviceGroup(device, syncedCameras);
    return syncedCameras;
  }

  isLegacyGeneratedDisplayName(camera, channelNumber) {
    if (camera.syncedDisplayName || camera.discovered !== true) return false;
    const suffix = ` CH-${String(channelNumber).padStart(2, "0")}`;
    if (!String(camera.displayName || "").endsWith(suffix)) return false;
    const previousDeviceName = camera.displayName.slice(0, -suffix.length);
    return camera.id === legacyChannelCameraId(previousDeviceName, channelNumber);
  }

  removeSyncedCamera(cameraId) {
    this.db
      .table("streamProfiles")
      .filter((profile) => profile.cameraId === cameraId)
      .forEach((profile) => this.db.delete("streamProfiles", profile.id));

    this.db.table("groups").slice().forEach((group) => {
      if (!Array.isArray(group.cameraIds) || !group.cameraIds.includes(cameraId)) return;
      this.db.upsert("groups", {
        ...group,
        cameraIds: group.cameraIds.filter((id) => id !== cameraId)
      });
    });

    this.db.delete("cameras", cameraId);
  }

  ensureDeviceGroup(device, cameras = this.list().filter((camera) => camera.deviceId === device.id)) {
    return ensureDeviceGroupRow(this.db, device, cameras);
  }

  deviceGroup(deviceOrId) {
    const id = deviceGroupId(deviceOrId);
    return this.db.table("groups").find((group) => group.id === id);
  }

  createDefaultProfiles(cameraId) {
    const profiles = [
      { tier: "thumb", resolution: "480p", fps: 6, bitrateKbps: 256, codec: "H.264" },
      { tier: "sub", resolution: "720p", fps: 15, bitrateKbps: 1000, codec: "H.264" },
      { tier: "main", resolution: "native", fps: 25, bitrateKbps: 4000, codec: "H.264/H.265" }
    ];

    profiles.forEach((profile) => {
      const profileId = `${cameraId}-${profile.tier}`;
      if (this.db.table("streamProfiles").some((existing) => existing.id === profileId)) return;
      this.db.upsert("streamProfiles", {
        id: profileId,
        cameraId,
        ...profile,
        isAvailable: true
      });
    });
  }
}

class DeviceOnboardingService {
  constructor(db) {
    this.db = db;
    this.devices = new DeviceService(db);
    this.cameras = new CameraService(db);
  }

  onboard(device) {
    const savedDevice = this.devices.save(device);
    const cameras = this.cameras.syncDeviceChannels(savedDevice);
    return {
      device: savedDevice,
      cameras,
      group: this.cameras.deviceGroup(savedDevice)
    };
  }

  syncDevice(deviceOrId) {
    const device =
      typeof deviceOrId === "string"
        ? this.devices.list().find((candidate) => candidate.id === deviceOrId)
        : deviceOrId;
    if (!device) throw new Error("Device not found.");
    const cameras = this.cameras.syncDeviceChannels(device);
    return {
      device,
      cameras,
      group: this.cameras.deviceGroup(device)
    };
  }
}

class MediaGuardService {
  constructor(db) {
    this.db = db;
  }

  activeUsage(deviceId) {
    const sessions = this.db
      .table("cameraSessions")
      .filter((session) => session.deviceId === deviceId && session.status !== "closed");

    return {
      main: sessions.filter((session) => session.tier === "main").length,
      sub: sessions.filter((session) => session.tier === "sub" || session.tier === "thumb").length,
      bitrateKbps: sessions.reduce((sum, session) => sum + Number(session.bitrateKbps || 0), 0)
    };
  }

  planOpen({ camera, device, paneContext, tileCount, isFocused = false, isTracking = false, zone, zoomLevel }) {
    let tier = resolveTier({ paneContext, tileCount, isFocused, isTracking, isVisible: true, zone, zoomLevel });
    const usage = this.activeUsage(device.id);

    if (tier === "main" && usage.main >= Number(device.maxConcurrentMainstream || 4)) {
      tier = "sub";
    }

    if ((tier === "sub" || tier === "thumb") && usage.sub >= Number(device.maxConcurrentSubstream || 32)) {
      tier = "paused";
    }

    return {
      cameraId: camera.id,
      deviceId: device.id,
      tier,
      bitrateKbps: estimateBitrateKbps(tier),
      reason: tier === "paused" ? "Device substream slots full" : "Allowed by device stream guard"
    };
  }
}

module.exports = {
  CameraService,
  DeviceOnboardingService,
  DeviceService,
  MediaGuardService,
  SUPPORTED_DEVICE_TYPES,
  deviceGroupId,
  ensureDeviceGroupRow,
  normalizeDeviceChannelCount,
  normalizeDeviceType
};
