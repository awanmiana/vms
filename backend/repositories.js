class DeviceRepository {
  constructor(db) {
    this.db = db;
  }

  listDevices() {
    throw new Error("DeviceRepository.listDevices requires a SQLite runtime.");
  }

  saveDevice() {
    throw new Error("DeviceRepository.saveDevice requires a SQLite runtime.");
  }
}

class CameraRepository {
  constructor(db) {
    this.db = db;
  }

  listCameras() {
    throw new Error("CameraRepository.listCameras requires a SQLite runtime.");
  }

  syncDeviceChannels() {
    throw new Error("CameraRepository.syncDeviceChannels requires a SQLite runtime.");
  }
}

class SessionRepository {
  constructor(db) {
    this.db = db;
  }

  createUserSession() {
    throw new Error("SessionRepository.createUserSession requires a SQLite runtime.");
  }

  revokeUserSessions() {
    throw new Error("SessionRepository.revokeUserSessions requires a SQLite runtime.");
  }
}

class MediaSessionRepository {
  constructor(db) {
    this.db = db;
  }

  listActiveSessionsByDevice() {
    throw new Error("MediaSessionRepository.listActiveSessionsByDevice requires a SQLite runtime.");
  }

  recordCameraSession() {
    throw new Error("MediaSessionRepository.recordCameraSession requires a SQLite runtime.");
  }
}

module.exports = {
  CameraRepository,
  DeviceRepository,
  MediaSessionRepository,
  SessionRepository
};
