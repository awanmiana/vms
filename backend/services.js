const { createId } = require("../shared/core");
const { defaultStreamProfiles, estimateBitrateKbps, resolveTier } = require("./media-policy");
const {
  SUPPORTED_DEVICE_TYPES,
  buildSystemDeviceGroup,
  deviceGroupId,
  normalizeDeviceChannelCount,
  normalizeDeviceType,
  reconcileDeviceChannels
} = require("../shared/inventory/contract");

function ensureDeviceGroupRow(repositories, device, cameras = repositories.cameras.listByDevice(device.id)) {
  const groupId = deviceGroupId(device);
  const existing = repositories.groups.findById(groupId);
  const group = buildSystemDeviceGroup(device, cameras, existing || {});
  repositories.groups.save(group);
  return repositories.groups.findById(groupId);
}

class DeviceService {
  constructor(repositories) {
    this.repositories = repositories;
  }

  list() {
    return this.repositories.devices.list();
  }

  save(device) {
    const type = normalizeDeviceType(device.type);
    const port = Number(device.port);
    if (!Number.isInteger(port) || port < 1 || port > 65535) {
      throw new Error("Device port must be an explicit integer from 1 to 65535.");
    }
    const row = {
      id: device.id || createId("dev"),
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

    this.repositories.devices.save(row);
    ensureDeviceGroupRow(this.repositories, row);
    return row;
  }
}

class CameraService {
  constructor(repositories) {
    this.repositories = repositories;
  }

  list() {
    return this.repositories.cameras.list();
  }

  syncDeviceChannels(device) {
    if (!device?.id) throw new Error("A saved device with an id is required to sync channels.");
    const result = reconcileDeviceChannels(device, this.repositories.cameras.list());
    result.removedIds.forEach((cameraId) => this.removeSyncedCamera(cameraId));
    const changedIds = new Set([
      ...result.deviceCameras.map((camera) => camera.id),
      ...result.detachedIds
    ]);
    result.cameras
      .filter((camera) => changedIds.has(camera.id))
      .forEach((camera) => {
        this.repositories.cameras.save(camera);
        if (camera.deviceId === device.id) this.createDefaultProfiles(camera.id);
      });

    const syncedCameras = this.list()
      .filter((camera) => camera.deviceId === device.id)
      .sort((left, right) => Number(left.channelNumber) - Number(right.channelNumber));
    this.ensureDeviceGroup(device, syncedCameras);
    return syncedCameras;
  }

  removeSyncedCamera(cameraId) {
    this.repositories.streamProfiles.listByCamera(cameraId)
      .forEach((profile) => this.repositories.streamProfiles.remove(profile.id));

    this.repositories.groups.list().slice().forEach((group) => {
      if (!Array.isArray(group.cameraIds) || !group.cameraIds.includes(cameraId)) return;
      this.repositories.groups.save({
        ...group,
        cameraIds: group.cameraIds.filter((id) => id !== cameraId)
      });
    });

    this.repositories.cameras.remove(cameraId);
  }

  ensureDeviceGroup(device, cameras = this.list().filter((camera) => camera.deviceId === device.id)) {
    return ensureDeviceGroupRow(this.repositories, device, cameras);
  }

  deviceGroup(deviceOrId) {
    const id = deviceGroupId(deviceOrId);
    return this.repositories.groups.findById(id);
  }

  createDefaultProfiles(cameraId) {
    defaultStreamProfiles(cameraId).forEach((profile) => {
      if (this.repositories.streamProfiles.findById(profile.id)) return;
      this.repositories.streamProfiles.save(profile);
    });
  }
}

class DeviceOnboardingService {
  constructor({ devices, cameras }) {
    this.devices = devices;
    this.cameras = cameras;
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
  constructor(repositories) {
    this.repositories = repositories;
  }

  activeUsage(deviceId) {
    const sessions = this.repositories.mediaSessions.listActiveByDevice(deviceId);

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
