const defaultDevices = [
  {
    id: "dev-main-building",
    name: "NVR-01 Main Building",
    type: "NVR",
    vendor: "Reference Vendor",
    host: "192.168.1.10",
    port: 8000,
    channelCount: 16,
    status: "online",
    maxConcurrentMainstream: 4,
    maxConcurrentSubstream: 32,
    notes: "Main gate, reception, lobby and parking cameras."
  }
];

const defaultUsers = [
  {
    id: "usr-admin",
    fullName: "System Admin",
    username: "admin",
    role: "Administrator",
    status: "Active",
    permissions: ["live", "playback", "incidents", "export", "devices", "admin"],
    groupIds: [],
    notes: "Prototype administrator account."
  }
];

function emptyStore() {
  return {
    version: 1,
    devices: [],
    cameras: [],
    streamProfiles: [],
    groups: [],
    users: [],
    sessions: [],
    cameraSessions: [],
    incidents: [],
    siteCanvases: [],
    cameraPositions: [],
    trackingSessions: [],
    sessionBreadcrumbs: [],
    voiceAliases: [],
    commandLog: [],
    entities: [],
    locations: [],
    cameraLocations: [],
    tagIndex: [],
    complianceTypes: [],
    complianceEvents: [],
    complianceLogs: [],
    complianceLogCameras: [],
    complianceLogLocations: [],
    tickets: [],
    operatorSettings: []
  };
}

module.exports = {
  defaultDevices,
  defaultUsers,
  emptyStore
};
