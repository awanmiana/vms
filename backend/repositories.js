class RepositoryContractError extends Error {
  constructor(message) {
    super(message);
    this.name = "RepositoryContractError";
    this.code = "REPOSITORY_CONTRACT_INVALID";
  }
}

class TableRepository {
  constructor(store, tableName) {
    if (!store || typeof store.table !== "function" || typeof store.upsert !== "function") {
      throw new RepositoryContractError(`${tableName} repository requires a table store adapter.`);
    }
    this.store = store;
    this.tableName = tableName;
  }

  list() {
    return this.store.table(this.tableName);
  }

  findById(id) {
    return this.list().find((row) => row.id === id) || null;
  }

  save(row) {
    this.store.upsert(this.tableName, row);
    return this.findById(row.id);
  }

  remove(id) {
    this.store.delete(this.tableName, id);
  }
}

class DeviceRepository extends TableRepository {
  constructor(store) { super(store, "devices"); }
}

class CameraRepository extends TableRepository {
  constructor(store) { super(store, "cameras"); }

  listByDevice(deviceId) {
    return this.list().filter((camera) => camera.deviceId === deviceId);
  }
}

class GroupRepository extends TableRepository {
  constructor(store) { super(store, "groups"); }
}

class StreamProfileRepository extends TableRepository {
  constructor(store) { super(store, "streamProfiles"); }

  listByCamera(cameraId) {
    return this.list().filter((profile) => profile.cameraId === cameraId);
  }
}

class SessionRepository extends TableRepository {
  constructor(store) { super(store, "sessions"); }
}

class MediaSessionRepository extends TableRepository {
  constructor(store) { super(store, "cameraSessions"); }

  listActiveByDevice(deviceId) {
    return this.list().filter((session) => session.deviceId === deviceId && session.status !== "closed");
  }
}

class InventoryRepository {
  constructor(store) {
    if (!store || typeof store.snapshot !== "function" || typeof store.replaceTables !== "function") {
      throw new RepositoryContractError("InventoryRepository requires snapshot and replace-table adapter methods.");
    }
    this.store = store;
  }

  isInitialized() {
    return this.store.record("inventoryState")?.initialized === true;
  }

  readTables() {
    return this.store.snapshot(["devices", "cameras", "groups"]);
  }

  replace({ devices, cameras, groups, inventoryState }) {
    this.store.replaceTables({ devices, cameras, groups }, { inventoryState });
  }
}

class FileStoreRepositoryContext {
  constructor(store) {
    this.store = store;
  }

  get persistenceFallback() {
    return this.store.persistenceFallback;
  }

  table(name) { return this.store.table(name); }
  upsert(name, row) { return this.store.upsert(name, row); }
  delete(name, id) { return this.store.delete(name, id); }
}

function createRepositories(store) {
  const context = new FileStoreRepositoryContext(store);
  return Object.freeze({
    cameras: new CameraRepository(store),
    devices: new DeviceRepository(store),
    groups: new GroupRepository(store),
    inventory: new InventoryRepository(store),
    mediaSessions: new MediaSessionRepository(store),
    sessions: new SessionRepository(store),
    store: context,
    streamProfiles: new StreamProfileRepository(store)
  });
}

module.exports = {
  CameraRepository,
  DeviceRepository,
  FileStoreRepositoryContext,
  GroupRepository,
  InventoryRepository,
  MediaSessionRepository,
  RepositoryContractError,
  SessionRepository,
  StreamProfileRepository,
  TableRepository,
  createRepositories
};
