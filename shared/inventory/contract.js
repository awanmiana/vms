(function exposeInventoryContract(root, factory) {
  const contract = factory();
  if (typeof module === "object" && module.exports) module.exports = contract;
  if (root) root.VmsInventoryContract = contract;
})(typeof globalThis !== "undefined" ? globalThis : this, function inventoryContractFactory() {
  "use strict";

  const INVENTORY_VERSION = 2;
  const SUPPORTED_DEVICE_TYPES = Object.freeze([
    "NVR",
    "DVR",
    "Hybrid DVR",
    "IP Camera Direct"
  ]);
  const deviceTypes = new Set(SUPPORTED_DEVICE_TYPES);
  const deviceStatuses = new Set(["online", "offline", "warning", "unknown"]);

  class InventoryValidationError extends Error {
    constructor(message) {
      super(message);
      this.name = "InventoryValidationError";
    }
  }

  function cleanText(value, { fallback = "", max = 256, required = false, label = "value" } = {}) {
    const text = value === undefined || value === null ? fallback : String(value);
    const cleaned = text.replace(/[\u0000-\u0008\u000B\u000C\u000E-\u001F\u007F]/g, "").trim();
    if (required && !cleaned) throw new InventoryValidationError(`${label} is required.`);
    if (cleaned.length > max) {
      throw new InventoryValidationError(`${label} must be ${max} characters or fewer.`);
    }
    return cleaned;
  }

  function cleanId(value, label = "id") {
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
    return [...new Set(value
      .slice(0, maxItems)
      .map((item) => cleanText(item, { max: maxLength }))
      .filter(Boolean))];
  }

  function normalizeDeviceType(value) {
    const type = String(value || "NVR").trim();
    if (!deviceTypes.has(type)) {
      throw new InventoryValidationError(`Unsupported device type: ${type || "(empty)"}`);
    }
    return type;
  }

  function normalizeChannelCount(value) {
    const parsed = Number(value);
    if (!Number.isFinite(parsed)) return 1;
    return Math.max(1, Math.min(256, Math.floor(parsed)));
  }

  function normalizeDeviceChannelCount(type, value) {
    return normalizeDeviceType(type) === "IP Camera Direct" ? 1 : normalizeChannelCount(value);
  }

  function channelDisplayName(device, channelNumber) {
    return `${device.name} CH-${String(channelNumber).padStart(2, "0")}`;
  }

  function channelCameraId(deviceOrId, channelNumber) {
    const deviceId = typeof deviceOrId === "string" ? deviceOrId : deviceOrId?.id;
    return `${cleanId(deviceId, "device id")}-CH${String(channelNumber).padStart(2, "0")}`;
  }

  function deviceGroupId(deviceOrId) {
    const deviceId = typeof deviceOrId === "string" ? deviceOrId : deviceOrId?.id;
    return `grp-device-${cleanId(deviceId, "device id")}`;
  }

  function cameraBelongsToDevice(camera, device) {
    if (!camera || !device) return false;
    if (camera.deviceId) return camera.deviceId === device.id;
    return camera.nvr === device.name;
  }

  function isManagedChannelPlaceholder(camera) {
    if (camera?.managedPlaceholder === true) return true;
    if (camera?.managedPlaceholder === false) return false;
    if (camera?.deviceSyncManaged === true) return true;
    return camera?.discovered === true
      && Array.isArray(camera.tags)
      && camera.tags.includes("unmapped")
      && (camera.area || "Unassigned") === "Unassigned"
      && (camera.floor || "Unknown") === "Unknown"
      && (camera.direction || "Direction not set") === "Direction not set";
  }

  function upgradeInventory(payload) {
    const source = payload?.inventory || payload;
    if (!source || typeof source !== "object" || Array.isArray(source)) {
      throw new InventoryValidationError("Request body must be a JSON object.");
    }
    const version = Number(source.version ?? source.inventoryVersion ?? 1);
    if (!Number.isInteger(version) || version < 1 || version > INVENTORY_VERSION) {
      throw new InventoryValidationError(`Unsupported inventory version: ${source.version}.`);
    }
    return {
      version: INVENTORY_VERSION,
      devices: Array.isArray(source.devices) ? source.devices.map((device) => ({
        ...device,
        channelCount: device?.channelCount ?? device?.channels,
        channels: device?.channels ?? device?.channelCount
      })) : source.devices,
      cameras: Array.isArray(source.cameras) ? source.cameras.map((camera) => ({
        ...camera,
        id: camera?.id ?? camera?.camera_id,
        deviceId: camera?.deviceId ?? camera?.device_id ?? "",
        name: camera?.name ?? camera?.displayName,
        channel: camera?.channel ?? camera?.channelNumber,
        channelNumber: camera?.channelNumber ?? camera?.channel
      })) : source.cameras,
      groups: Array.isArray(source.groups) ? source.groups.map((group) => ({
        ...group,
        id: group?.id ?? group?.group_id,
        grid: group?.grid ?? group?.preferredGrid ?? group?.preferred_grid,
        cameraIds: group?.cameraIds ?? group?.camera_ids,
        system: group?.system ?? group?.isSystem ?? group?.is_system,
        deviceId: group?.deviceId ?? group?.device_id ?? ""
      })) : source.groups
    };
  }

  function rejectDuplicate(seen, value, label) {
    if (seen.has(value)) throw new InventoryValidationError(`Duplicate ${label}: ${value}.`);
    seen.add(value);
  }

  function requireArray(payload, key) {
    if (!Array.isArray(payload[key])) throw new InventoryValidationError(`${key} must be an array.`);
    return payload[key];
  }

  function normalizeDevice(raw, index, seenIds, seenNames) {
    if (!raw || typeof raw !== "object" || Array.isArray(raw)) {
      throw new InventoryValidationError(`devices[${index}] must be an object.`);
    }
    const id = cleanId(raw.id, `devices[${index}].id`);
    const name = cleanText(raw.name, { required: true, max: 160, label: `devices[${index}].name` });
    rejectDuplicate(seenIds, id, "device id");
    rejectDuplicate(seenNames, name.toLowerCase(), "device name");
    const type = normalizeDeviceType(raw.type);
    const status = cleanText(raw.status, { fallback: "unknown", max: 16 }).toLowerCase();
    if (!deviceStatuses.has(status)) {
      throw new InventoryValidationError(`devices[${index}].status is not supported.`);
    }
    const channels = normalizeDeviceChannelCount(type, integerInRange(
      raw.channels ?? raw.channelCount, 1, 1, 256, `devices[${index}].channels`
    ));
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
      password: "",
      notes: cleanText(raw.notes, { max: 4000 }),
      maxConcurrentMainstream: integerInRange(raw.maxConcurrentMainstream, 4, 1, 256,
        `devices[${index}].maxConcurrentMainstream`),
      maxConcurrentSubstream: integerInRange(raw.maxConcurrentSubstream, 32, 1, 1024,
        `devices[${index}].maxConcurrentSubstream`)
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
    const device = devicesById.get(suppliedDeviceId)
      || devicesByName.get(suppliedDeviceName.toLowerCase());
    const channel = integerInRange(raw.channel ?? raw.channelNumber, 1, 1, 4096,
      `cameras[${index}].channel`);
    if (device) {
      if (channel > device.channels) {
        throw new InventoryValidationError(
          `cameras[${index}].channel exceeds the configured channel count for ${device.name}.`
        );
      }
      rejectDuplicate(seenDeviceChannels, `${device.id}:${channel}`, "device camera channel");
    }
    const name = cleanText(raw.name ?? raw.displayName, {
      required: true, max: 160, label: `cameras[${index}].name`
    });
    const status = cleanText(raw.status, { fallback: "unknown", max: 16 }).toLowerCase();
    if (!deviceStatuses.has(status)) {
      throw new InventoryValidationError(`cameras[${index}].status is not supported.`);
    }
    const tags = uniqueStrings(raw.tags);
    const area = cleanText(raw.area, { fallback: "Unassigned", max: 160 });
    const floor = cleanText(raw.floor, { fallback: "Unknown", max: 128 });
    const direction = cleanText(raw.direction, { fallback: "Direction not set", max: 512 });
    const managedPlaceholder = isManagedChannelPlaceholder({ ...raw, tags, area, floor, direction });
    return {
      ...raw,
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
      system: false,
      isSystem: false,
      deviceId: ""
    };
  }

  function buildSystemDeviceGroup(device, cameras, existing = {}) {
    const cameraIds = cameras
      .filter((camera) => cameraBelongsToDevice(camera, device))
      .slice()
      .sort((left, right) => Number(left.channel ?? left.channelNumber)
        - Number(right.channel ?? right.channelNumber) || String(left.id).localeCompare(String(right.id)))
      .map((camera) => camera.id);
    return {
      ...existing,
      id: deviceGroupId(device),
      name: `${device.name} (Assigned)`,
      purpose: existing.purpose || "Auto-assigned device channels",
      grid: Number(existing.grid || 4),
      cameraIds,
      notes: existing.notes || "Created automatically from this device's channels.",
      system: true,
      isSystem: true,
      deviceId: device.id
    };
  }

  function buildSystemDeviceGroups(devices, cameras, groups = []) {
    const devicesById = new Set(devices.map((device) => device.id));
    const custom = groups.filter((group) => !(group.system || group.isSystem || group.deviceId));
    const existingByDevice = new Map(groups
      .filter((group) => (group.system || group.isSystem) && devicesById.has(group.deviceId))
      .map((group) => [group.deviceId, group]));
    return [...custom, ...devices.map((device) =>
      buildSystemDeviceGroup(device, cameras, existingByDevice.get(device.id) || {}))];
  }

  function normalizeInventory(payload) {
    const upgraded = upgradeInventory(payload);
    const deviceIds = new Set();
    const deviceNames = new Set();
    const devices = requireArray(upgraded, "devices").map((device, index) =>
      normalizeDevice(device, index, deviceIds, deviceNames));
    const devicesById = new Map(devices.map((device) => [device.id, device]));
    const devicesByName = new Map(devices.map((device) => [device.name.toLowerCase(), device]));
    const cameraIds = new Set();
    const deviceChannels = new Set();
    const cameras = requireArray(upgraded, "cameras").map((camera, index) =>
      normalizeCamera(camera, index, devicesById, devicesByName, cameraIds, deviceChannels));
    cameras.forEach((camera) => {
      camera.related = camera.related.filter((id) => cameraIds.has(id));
      camera.next = camera.next.filter((id) => cameraIds.has(id));
      camera.previous = camera.previous.filter((id) => cameraIds.has(id));
    });
    const reservedSystemIds = new Set(devices.map(deviceGroupId));
    const customGroupIds = new Set();
    const customGroups = requireArray(upgraded, "groups")
      .map((group, index) => normalizeCustomGroup(
        group, index, cameraIds, reservedSystemIds, customGroupIds
      ))
      .filter(Boolean);
    return {
      version: INVENTORY_VERSION,
      devices,
      cameras,
      groups: buildSystemDeviceGroups(devices, cameras, customGroups)
    };
  }

  function nextAvailableCameraId(device, channelNumber, cameras) {
    const base = channelCameraId(device, channelNumber);
    const ids = new Set(cameras.map((camera) => camera.id));
    if (!ids.has(base)) return base;
    let suffix = 2;
    while (ids.has(`${base}-${suffix}`)) suffix += 1;
    return `${base}-${suffix}`;
  }

  function reconcileDeviceChannels(device, cameras) {
    if (!device?.id) throw new InventoryValidationError("A saved device with an id is required to sync channels.");
    const channelCount = normalizeDeviceChannelCount(
      device.type, device.channels ?? device.channelCount ?? 1
    );
    const untouched = [];
    const candidatesByChannel = new Map();
    const removedIds = [];
    const detachedIds = [];
    for (const source of cameras) {
      if (!cameraBelongsToDevice(source, device)) {
        untouched.push(source);
        continue;
      }
      const channel = Number(source.channel ?? source.channelNumber);
      const managed = isManagedChannelPlaceholder(source);
      if (!Number.isInteger(channel) || channel < 1 || channel > channelCount) {
        if (managed) {
          removedIds.push(source.id);
        } else {
          detachedIds.push(source.id);
          untouched.push({
            ...source,
            deviceId: "",
            nvr: "Unassigned",
            managedPlaceholder: false,
            deviceSyncManaged: false
          });
        }
        continue;
      }
      const list = candidatesByChannel.get(channel) || [];
      list.push(source);
      candidatesByChannel.set(channel, list);
    }

    const reconciled = [];
    for (let channel = 1; channel <= channelCount; channel += 1) {
      const candidates = candidatesByChannel.get(channel) || [];
      if (candidates.length) {
        const keep = candidates.find((camera) => !isManagedChannelPlaceholder(camera)) || candidates[0];
        for (const duplicate of candidates) {
          if (duplicate === keep) continue;
          if (isManagedChannelPlaceholder(duplicate)) removedIds.push(duplicate.id);
          else {
            detachedIds.push(duplicate.id);
            untouched.push({
              ...duplicate,
              deviceId: "",
              nvr: "Unassigned",
              managedPlaceholder: false,
              deviceSyncManaged: false
            });
          }
        }
        const managed = isManagedChannelPlaceholder(keep);
        const generatedName = channelDisplayName(device, channel);
        const displayNameWasManaged = !keep.displayName
          || keep.displayName === keep.syncedDisplayName
          || (managed && !keep.syncedDisplayName);
        const effectiveName = displayNameWasManaged
          ? generatedName
          : (keep.name || keep.displayName);
        const effectiveDisplayName = displayNameWasManaged
          ? generatedName
          : (keep.displayName || keep.name);
        reconciled.push({
          ...keep,
          deviceId: device.id,
          nvr: device.name,
          channel,
          channelNumber: channel,
          name: effectiveName,
          displayName: effectiveDisplayName,
          syncedDisplayName: generatedName,
          syncedDeviceName: device.name,
          managedPlaceholder: managed,
          deviceSyncManaged: managed
        });
        continue;
      }
      const generatedName = channelDisplayName(device, channel);
      reconciled.push({
        id: nextAvailableCameraId(device, channel, [...untouched, ...reconciled]),
        deviceId: device.id,
        nvr: device.name,
        channel,
        channelNumber: channel,
        name: generatedName,
        displayName: generatedName,
        syncedDisplayName: generatedName,
        syncedDeviceName: device.name,
        area: "Unassigned",
        floor: "Unknown",
        direction: "Direction not set",
        status: "unknown",
        tags: ["unmapped"],
        related: [],
        next: [],
        previous: [],
        discovered: true,
        managedPlaceholder: true,
        deviceSyncManaged: true
      });
    }

    const removed = new Set(removedIds);
    const resultCameras = [...untouched, ...reconciled].map((camera) => ({
      ...camera,
      related: (camera.related || []).filter((id) => !removed.has(id)),
      next: (camera.next || []).filter((id) => !removed.has(id)),
      previous: (camera.previous || []).filter((id) => !removed.has(id))
    }));
    return {
      cameras: resultCameras,
      deviceCameras: reconciled,
      removedIds,
      detachedIds,
      addedCount: reconciled.filter((camera) => !cameras.some((old) => old.id === camera.id)).length,
      removedCount: removedIds.length,
      detachedCount: detachedIds.length,
      channelCount
    };
  }

  return Object.freeze({
    INVENTORY_VERSION,
    InventoryValidationError,
    SUPPORTED_DEVICE_TYPES,
    buildSystemDeviceGroup,
    buildSystemDeviceGroups,
    cameraBelongsToDevice,
    channelCameraId,
    channelDisplayName,
    cleanId,
    deviceGroupId,
    isManagedChannelPlaceholder,
    normalizeDeviceChannelCount,
    normalizeDeviceType,
    normalizeInventory,
    reconcileDeviceChannels,
    upgradeInventory
  });
});
