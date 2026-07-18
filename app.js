const isVmsTestMode = typeof globalThis !== "undefined" && globalThis.__VMS_TEST_MODE__ === true;

const defaultDevices = [
  {
    id: "dev-1001",
    name: "NVR-01 Main Building",
    type: "NVR",
    vendor: "Reference Vendor",
    host: "192.168.1.10",
    port: 8000,
    channels: 16,
    status: "online",
    username: "admin",
    password: "",
    notes: "Main gate, reception, lobby and parking cameras."
  },
  {
    id: "dev-1002",
    name: "NVR-02 Admin Block",
    type: "NVR",
    vendor: "Reference Vendor",
    host: "192.168.1.11",
    port: 8000,
    channels: 16,
    status: "warning",
    username: "admin",
    password: "",
    notes: "One corridor camera needs naming verification."
  },
  {
    id: "dev-1003",
    name: "DVR-03 First Floor",
    type: "DVR",
    vendor: "Alternate Vendor",
    host: "192.168.1.12",
    port: 37777,
    channels: 32,
    status: "online",
    username: "admin",
    password: "",
    notes: "First floor and lift landing."
  }
];

const defaultCameras = [
  {
    id: "MG-01",
    name: "Main Gate Entry",
    area: "Main Gate",
    floor: "Ground",
    direction: "Facing inbound vehicle lane",
    deviceId: "dev-1001",
    nvr: "NVR-01 Main Building",
    channel: 1,
    stream: "rtsp://placeholder/main-gate-entry",
    status: "online",
    tags: ["gate", "entry", "vehicle", "security"],
    related: ["MG-02", "REC-01", "PARK-01"],
    next: ["REC-01", "PARK-01"],
    previous: ["MG-02"]
  },
  {
    id: "MG-02",
    name: "Main Gate Exit",
    area: "Main Gate",
    floor: "Ground",
    direction: "Facing outbound lane",
    deviceId: "dev-1001",
    nvr: "NVR-01 Main Building",
    channel: 2,
    stream: "rtsp://placeholder/main-gate-exit",
    status: "online",
    tags: ["gate", "exit", "vehicle"],
    related: ["MG-01", "PARK-02"],
    next: ["MG-01"],
    previous: ["PARK-02"]
  },
  {
    id: "REC-01",
    name: "Reception Entrance",
    area: "Reception",
    floor: "Ground",
    direction: "Facing front doors",
    deviceId: "dev-1001",
    nvr: "NVR-01 Main Building",
    channel: 3,
    stream: "rtsp://placeholder/reception-entrance",
    status: "online",
    tags: ["reception", "entry", "person"],
    related: ["MG-01", "LOB-01", "COR-01"],
    next: ["LOB-01", "COR-01"],
    previous: ["MG-01"]
  },
  {
    id: "LOB-01",
    name: "Lobby Overview",
    area: "Lobby",
    floor: "Ground",
    direction: "Wide view of lobby",
    deviceId: "dev-1001",
    nvr: "NVR-01 Main Building",
    channel: 4,
    stream: "rtsp://placeholder/lobby-overview",
    status: "online",
    tags: ["lobby", "overview", "person"],
    related: ["REC-01", "COR-01", "LIFT-01"],
    next: ["LIFT-01", "COR-01"],
    previous: ["REC-01"]
  },
  {
    id: "COR-01",
    name: "Admin Corridor",
    area: "Admin Block",
    floor: "Ground",
    direction: "Facing offices",
    deviceId: "dev-1002",
    nvr: "NVR-02 Admin Block",
    channel: 1,
    stream: "rtsp://placeholder/admin-corridor",
    status: "warning",
    tags: ["corridor", "admin", "person"],
    related: ["REC-01", "LOB-01", "ADM-01"],
    next: ["ADM-01"],
    previous: ["REC-01", "LOB-01"]
  },
  {
    id: "ADM-01",
    name: "Admin Office Entrance",
    area: "Admin Block",
    floor: "Ground",
    direction: "Facing admin entrance",
    deviceId: "dev-1002",
    nvr: "NVR-02 Admin Block",
    channel: 2,
    stream: "rtsp://placeholder/admin-office",
    status: "online",
    tags: ["admin", "office", "entry"],
    related: ["COR-01"],
    next: ["COR-01"],
    previous: ["COR-01"]
  },
  {
    id: "LIFT-01",
    name: "Lift Lobby",
    area: "Lift Area",
    floor: "Ground",
    direction: "Facing lift doors",
    deviceId: "dev-1002",
    nvr: "NVR-02 Admin Block",
    channel: 3,
    stream: "rtsp://placeholder/lift-lobby",
    status: "online",
    tags: ["lift", "lobby", "person"],
    related: ["LOB-01", "F1-01"],
    next: ["F1-01"],
    previous: ["LOB-01"]
  },
  {
    id: "F1-01",
    name: "First Floor Lift Exit",
    area: "First Floor",
    floor: "First",
    direction: "Facing first floor lift landing",
    deviceId: "dev-1003",
    nvr: "DVR-03 First Floor",
    channel: 1,
    stream: "rtsp://placeholder/first-floor-lift",
    status: "online",
    tags: ["lift", "floor-1", "person"],
    related: ["LIFT-01", "F1-02"],
    next: ["F1-02"],
    previous: ["LIFT-01"]
  },
  {
    id: "F1-02",
    name: "First Floor Corridor",
    area: "First Floor",
    floor: "First",
    direction: "Facing conference rooms",
    deviceId: "dev-1003",
    nvr: "DVR-03 First Floor",
    channel: 2,
    stream: "rtsp://placeholder/first-floor-corridor",
    status: "online",
    tags: ["corridor", "floor-1", "person"],
    related: ["F1-01"],
    next: ["F1-01"],
    previous: ["F1-01"]
  },
  {
    id: "PARK-01",
    name: "Parking Entry Row",
    area: "Parking",
    floor: "Ground",
    direction: "Facing entry parking row",
    deviceId: "dev-1001",
    nvr: "NVR-01 Main Building",
    channel: 5,
    stream: "rtsp://placeholder/parking-entry",
    status: "online",
    tags: ["parking", "entry", "vehicle"],
    related: ["MG-01", "PARK-02"],
    next: ["PARK-02"],
    previous: ["MG-01"]
  },
  {
    id: "PARK-02",
    name: "Parking Exit Row",
    area: "Parking",
    floor: "Ground",
    direction: "Facing parking exit",
    deviceId: "dev-1001",
    nvr: "NVR-01 Main Building",
    channel: 6,
    stream: "rtsp://placeholder/parking-exit",
    status: "online",
    tags: ["parking", "exit", "vehicle"],
    related: ["PARK-01", "MG-02"],
    next: ["MG-02"],
    previous: ["PARK-01"]
  }
];

const savedDevices = JSON.parse(localStorage.getItem("devices") || "null");
const savedCameras = JSON.parse(localStorage.getItem("cameras") || "null");
const savedMapPositions = JSON.parse(localStorage.getItem("mapPositions") || "null");
const savedIncidents = JSON.parse(localStorage.getItem("incidents") || "null");
const savedGroups = JSON.parse(localStorage.getItem("cameraGroups") || "null");
const savedUsers = JSON.parse(localStorage.getItem("users") || "null");
const savedLiveViewport = JSON.parse(localStorage.getItem("liveViewport") || "null");
const savedOpenCameraIds = JSON.parse(localStorage.getItem("openCameraIds") || "[]");
const restoreLastCameraGrid = localStorage.getItem("restoreLastCameraGrid") !== "false";
const savedDraggableCanvasMode = localStorage.getItem("useDraggableCanvases");
const useDraggableCanvases =
  savedDraggableCanvasMode === null
    ? localStorage.getItem("showLiveTracking") !== "false"
    : savedDraggableCanvasMode !== "false";

const defaultGroups = [
  {
    id: "grp-main-gate",
    name: "Main Gate Tracking",
    purpose: "Entry and exit movement",
    grid: 4,
    cameraIds: ["MG-01", "MG-02", "REC-01", "PARK-01"],
    notes: "Use when tracking people or vehicles entering from the main gate."
  },
  {
    id: "grp-lobby-admin",
    name: "Lobby to Admin",
    purpose: "Reception, lobby, and admin corridor",
    grid: 4,
    cameraIds: ["REC-01", "LOB-01", "COR-01", "ADM-01"],
    notes: "Useful for following movement from reception into admin offices."
  }
];

const INITIAL_ROLE = "Administrator";
const FEATURE_CAPABILITIES = Object.freeze(["live", "playback", "incidents", "export", "devices", "admin"]);

const defaultUsers = [
  {
    id: "usr-admin",
    name: "System Admin",
    username: "admin",
    role: INITIAL_ROLE,
    status: "Active",
    groupIds: ["grp-main-gate", "grp-lobby-admin"],
    permissions: [...FEATURE_CAPABILITIES],
    notes: "Prototype administrator account."
  }
];

function normalizeAdministratorUsers(users) {
  const source = Array.isArray(users) && users.length ? users : defaultUsers;
  return source.map((user) => ({
    ...user,
    role: INITIAL_ROLE,
    permissions: [...FEATURE_CAPABILITIES]
  }));
}

const validViews = new Set(["dashboard", "operator", "playback", "incidents", "devices", "map", "reports", "access", "settings", "catalog", "routes", "groups", "compliance"]);
const initialHashView = window.location.hash ? window.location.hash.slice(1) : "";
const legacyPlaybackOpenIds = JSON.parse(localStorage.getItem("playbackOpenIds") || "[]");
const legacyPlaybackRangeStart = localStorage.getItem("playbackRangeStart") || `${todayValue()} 00:00:00`;
const legacyPlaybackRangeEnd = localStorage.getItem("playbackRangeEnd") || `${todayValue()} 23:59:59`;

function normalizeWorkspaceList(kind, stored, legacy) {
  const source = Array.isArray(stored) && stored.length ? stored : [legacy];
  const usedIds = new Set();
  return source.map((workspace, index) => {
    const baseId = String(workspace?.id || `${kind}-${index + 1}`).trim() || `${kind}-${index + 1}`;
    let id = baseId;
    let suffix = 2;
    while (usedIds.has(id)) {
      id = `${baseId}-${suffix}`;
      suffix += 1;
    }
    usedIds.add(id);

    const rawFloatRect = workspace?.floatRect;
    const floatRect = rawFloatRect && typeof rawFloatRect === "object" &&
      ["x", "y", "width", "height"].every((key) => Number.isFinite(Number(rawFloatRect[key])))
      ? {
          x: Math.max(8, Number(rawFloatRect.x)),
          y: Math.max(8, Number(rawFloatRect.y)),
          width: Math.max(360, Number(rawFloatRect.width)),
          height: Math.max(280, Number(rawFloatRect.height))
        }
      : null;

    return {
      id,
      name: String(workspace?.name || `${kind === "live" ? "Live" : "Playback"} ${index + 1}`),
      openIds: Array.isArray(workspace?.openIds)
        ? [...new Set(workspace.openIds.map(String).filter(Boolean))]
        : [],
      selectedId: String(workspace?.selectedId || ""),
      gridDivision: Math.max(
        1,
        Math.min(kind === "live" ? 64 : 16, Math.round(Number(workspace?.gridDivision) || (kind === "live" ? 9 : 4)))
      ),
      rangeStart: kind === "playback" ? String(workspace?.rangeStart || legacyPlaybackRangeStart) : undefined,
      rangeEnd: kind === "playback" ? String(workspace?.rangeEnd || legacyPlaybackRangeEnd) : undefined,
      floating: workspace?.floating === true,
      layer: workspace?.layer === "back" ? "back" : "front",
      floatRect
    };
  });
}

const savedLiveWorkspaces = normalizeWorkspaceList(
  "live",
  JSON.parse(localStorage.getItem("liveWorkspaces") || "null"),
  {
    id: "live-1",
    name: "Live 1",
    openIds: restoreLastCameraGrid && Array.isArray(savedOpenCameraIds) ? savedOpenCameraIds : [],
    selectedId: localStorage.getItem("selectedCamera") || defaultCameras[0].id,
    gridDivision: Number(localStorage.getItem("gridDivision")) || 9
  }
);
const savedPlaybackWorkspaces = normalizeWorkspaceList(
  "playback",
  JSON.parse(localStorage.getItem("playbackWorkspaces") || "null"),
  {
    id: "playback-1",
    name: "Playback 1",
    openIds: Array.isArray(legacyPlaybackOpenIds) ? legacyPlaybackOpenIds : [],
    selectedId: localStorage.getItem("playbackSelectedCamera") || "",
    gridDivision: Number(localStorage.getItem("playbackGridDivision")) || 4,
    rangeStart: legacyPlaybackRangeStart,
    rangeEnd: legacyPlaybackRangeEnd
  }
);
const savedActiveLiveWorkspaceId = localStorage.getItem("activeLiveWorkspaceId");
const savedActivePlaybackWorkspaceId = localStorage.getItem("activePlaybackWorkspaceId");
const initialActiveLiveWorkspace =
  savedLiveWorkspaces.find((workspace) => workspace.id === savedActiveLiveWorkspaceId) || savedLiveWorkspaces[0];
const initialActivePlaybackWorkspace =
  savedPlaybackWorkspaces.find((workspace) => workspace.id === savedActivePlaybackWorkspaceId) || savedPlaybackWorkspaces[0];

const state = {
  selectedId: initialActiveLiveWorkspace.selectedId || defaultCameras[0].id,
  area: "All",
  tag: "All",
  query: "",
  gridSize: 4,
  openIds: [...initialActiveLiveWorkspace.openIds],
  liveViewport: savedLiveViewport && typeof savedLiveViewport === "object" ? savedLiveViewport : { centerX: 50, centerY: 50, zoom: 1 },
  devices: Array.isArray(savedDevices) ? savedDevices : defaultDevices,
  cameras: Array.isArray(savedCameras) ? savedCameras : defaultCameras,
  view: validViews.has(initialHashView) ? initialHashView : localStorage.getItem("activeView") || "operator",
  theme: localStorage.getItem("theme") || "dark",
  routeSelectedId: localStorage.getItem("routeSelectedCamera") || localStorage.getItem("selectedCamera") || defaultCameras[0].id,
  routeQuery: "",
  routeArea: "All",
  mapSelectedId: localStorage.getItem("mapSelectedCamera") || localStorage.getItem("selectedCamera") || defaultCameras[0].id,
  mapArea: localStorage.getItem("mapArea") || "All",
  mapPositions: savedMapPositions && typeof savedMapPositions === "object" ? savedMapPositions : {},
  playbackOpenIds: [...initialActivePlaybackWorkspace.openIds],
  playbackGridDivision: initialActivePlaybackWorkspace.gridDivision,
  playbackSelectedId: initialActivePlaybackWorkspace.selectedId,
  playbackQuery: "",
  playbackRangeStart: initialActivePlaybackWorkspace.rangeStart,
  playbackRangeEnd: initialActivePlaybackWorkspace.rangeEnd,
  incidents: Array.isArray(savedIncidents) ? savedIncidents : [],
  selectedIncidentId: localStorage.getItem("selectedIncident") || "",
  incidentStatus: "All",
  incidentQuery: "",
  groups: Array.isArray(savedGroups) ? savedGroups : defaultGroups,
  selectedGroupId: localStorage.getItem("selectedGroup") || "",
  users: normalizeAdministratorUsers(savedUsers),
  selectedUserId: localStorage.getItem("selectedUser") || "",
  showMediaPolicy: localStorage.getItem("showMediaPolicy") !== "false",
  useDraggableCanvases,
  restoreLastCameraGrid,
  liveWorkspaces: savedLiveWorkspaces,
  playbackWorkspaces: savedPlaybackWorkspaces,
  activeLiveWorkspaceId: initialActiveLiveWorkspace.id,
  activePlaybackWorkspaceId: initialActivePlaybackWorkspace.id,
  resourceOptimizationMode: localStorage.getItem("resourceOptimizationMode") === "manual" ? "manual" : "auto",
  maxActiveStreams: Number(localStorage.getItem("maxActiveStreams")) || 6,
  gridDivision: initialActiveLiveWorkspace.gridDivision,
  liveDigitalZoom: Number(localStorage.getItem("liveDigitalZoom")) || 1
};

let byId = new Map();
let areas = [];
let tags = [];
let liveSpatialCanvas = null;
let playbackSpatialCanvas = null;
let pendingLiveFrameIds = [];
let maximizedLiveCameraId = "";
let maximizedPlaybackCameraId = "";
let workspaceIdSequence = 0;
let checkedCameraIds = new Set();
let groupFormMemberIds = [];
let pendingCameraLinkMigration = false;
const INVENTORY_CONFIRMED_FINGERPRINT_KEY = "inventoryConfirmedFingerprintV1";
const INVENTORY_LAST_SUCCESS_KEY = "inventoryLastSuccessfulAtV1";
const INVENTORY_PENDING_SNAPSHOT_KEY = "inventoryPendingSnapshotV1";
let inventoryBackendReady = false;
let inventoryAuthorityState = inventoryApiAvailable() ? "checking" : "local_only";
let inventoryAuthorityDetail = "";
let inventoryLastSuccessfulAt = localStorage.getItem(INVENTORY_LAST_SUCCESS_KEY) || "";
let inventoryPendingSnapshot = localStorage.getItem(INVENTORY_PENDING_SNAPSHOT_KEY) || "";
let inventorySyncTimer = 0;
let inventorySyncChain = Promise.resolve();
let lastInventoryFingerprint = localStorage.getItem(INVENTORY_CONFIRMED_FINGERPRINT_KEY) || "";
let scheduledInventoryFingerprint = "";
let scheduledInventorySnapshot = "";

const viewCopy = {
  dashboard: ["Overview", "Workspace"],
  operator: ["Operate", "Monitor"],
  devices: ["Manage", "Devices"],
  catalog: ["Camera identity", "Camera Catalog"],
  routes: ["Tracking logic", "Route Builder"],
  map: ["Manage", "Map"],
  playback: ["Operate", "Playback"],
  incidents: ["Operate", "Events"],
  groups: ["Layouts", "Camera Groups"],
  access: ["Security", "Access"],
  compliance: ["Reports", "Compliance"],
  reports: ["Reports", "Reports"],
  settings: ["Configure", "Settings"]
};

const els = {
  appShell: document.getElementById("appShell"),
  sidebarToggle: document.getElementById("sidebarToggle"),
  statusDeviceCount: document.getElementById("statusDeviceCount"),
  statusCameraCount: document.getElementById("statusCameraCount"),
  statusMessage: document.getElementById("statusMessage"),
  inventoryRetryBtn: document.getElementById("inventoryRetryBtn"),
  statusClearBtn: document.getElementById("statusClearBtn"),
  statusThemeBtn: document.getElementById("statusThemeBtn"),
  navItems: document.querySelectorAll("[data-view]"),
  dashboardCards: document.querySelectorAll("[data-dashboard-view]"),
  appViews: document.querySelectorAll(".app-view"),
  areaFilters: document.getElementById("areaFilters"),
  tagFilters: document.getElementById("tagFilters"),
  cameraSearch: document.getElementById("cameraSearch"),
  mediaPolicyToggle: document.getElementById("mediaPolicyToggle"),
  draggableCanvasesToggle: document.getElementById("draggableCanvasesToggle"),
  streamingSettingsSection: document.getElementById("streamingSettingsSection"),
  resourceOptimizationMode: document.getElementById("resourceOptimizationMode"),
  resourceBudgetSummary: document.getElementById("resourceBudgetSummary"),
  restoreLastCameraGridToggle: document.getElementById("restoreLastCameraGridToggle"),
  maxActiveStreamsInput: document.getElementById("maxActiveStreamsInput"),
  cameraList: document.getElementById("cameraList"),
  cameraCount: document.getElementById("cameraCount"),
  livePanel: document.querySelector(".live-panel"),
  spatialCanvasEl: document.getElementById("spatialCanvas"),
  simpleLiveGrid: document.getElementById("simpleLiveGrid"),
  liveCanvasWrap: document.getElementById("liveCanvasWrap"),
  resourceTabs: document.querySelectorAll("[data-resource-tab]"),
  resourcePane: document.getElementById("resourcePane"),
  autoSwitchPane: document.getElementById("autoSwitchPane"),
  treeSearchInput: document.getElementById("treeSearchInput"),
  gridLayoutBtn: document.getElementById("gridLayoutBtn"),
  gridZoomOutBtn: document.getElementById("gridZoomOutBtn"),
  gridZoomInBtn: document.getElementById("gridZoomInBtn"),
  gridPtzBtn: document.getElementById("gridPtzBtn"),
  gridCloseAllBtn: document.getElementById("gridCloseAllBtn"),
  gridFullscreenBtn: document.getElementById("gridFullscreenBtn"),
  gridSettingsBtn: document.getElementById("gridSettingsBtn"),
  layoutPopover: document.getElementById("layoutPopover"),
  ptzPopover: document.getElementById("ptzPopover"),
  ptzStatus: document.getElementById("ptzStatus"),
  gridDivisionCustomInput: document.getElementById("gridDivisionCustomInput"),
  gridDivisionCustomBtn: document.getElementById("gridDivisionCustomBtn"),
  streamSettingsPopover: document.getElementById("streamSettingsPopover"),
  maxActiveStreamsInputGrid: document.getElementById("maxActiveStreamsInputGrid"),
  monitorDrawerTitle: document.getElementById("monitorDrawerTitle"),
  monitorDrawerStatus: document.getElementById("monitorDrawerStatus"),
  monitorDrawerBody: document.getElementById("monitorDrawerBody"),
  liveWorkspaceTabs: document.getElementById("liveWorkspaceTabs"),
  liveGroupList: document.getElementById("liveGroupList"),
  quickGroupNameInput: document.getElementById("quickGroupNameInput"),
  createLiveGroupBtn: document.getElementById("createLiveGroupBtn"),
  addCheckedToGroupBtn: document.getElementById("addCheckedToGroupBtn"),
  checkedCameraCount: document.getElementById("checkedCameraCount"),
  deviceForm: document.getElementById("deviceForm"),
  deviceId: document.getElementById("deviceId"),
  deviceName: document.getElementById("deviceName"),
  deviceType: document.getElementById("deviceType"),
  deviceVendor: document.getElementById("deviceVendor"),
  deviceHost: document.getElementById("deviceHost"),
  devicePort: document.getElementById("devicePort"),
  deviceChannels: document.getElementById("deviceChannels"),
  deviceStatus: document.getElementById("deviceStatus"),
  deviceUsername: document.getElementById("deviceUsername"),
  devicePassword: document.getElementById("devicePassword"),
  deviceNotes: document.getElementById("deviceNotes"),
  resetDeviceFormBtn: document.getElementById("resetDeviceFormBtn"),
  deviceTableBody: document.getElementById("deviceTableBody"),
  deviceCount: document.getElementById("deviceCount"),
  deviceFilterInput: document.getElementById("deviceFilterInput"),
  deviceSelectAll: document.getElementById("deviceSelectAll"),
  deviceAddBtn: document.getElementById("deviceAddBtn"),
  deviceDeleteBtn: document.getElementById("deviceDeleteBtn"),
  deviceRefreshBtn: document.getElementById("deviceRefreshBtn"),
  deviceExportBtn: document.getElementById("deviceExportBtn"),
  cameraForm: document.getElementById("cameraForm"),
  cameraEditId: document.getElementById("cameraEditId"),
  cameraIdInput: document.getElementById("cameraIdInput"),
  cameraChannelInput: document.getElementById("cameraChannelInput"),
  cameraNameInput: document.getElementById("cameraNameInput"),
  cameraDeviceInput: document.getElementById("cameraDeviceInput"),
  cameraChannelSelect: document.getElementById("cameraChannelSelect"),
  cameraAreaInput: document.getElementById("cameraAreaInput"),
  cameraFloorInput: document.getElementById("cameraFloorInput"),
  cameraDirectionInput: document.getElementById("cameraDirectionInput"),
  cameraTagsInput: document.getElementById("cameraTagsInput"),
  cameraRelatedInput: document.getElementById("cameraRelatedInput"),
  cameraPreviousInput: document.getElementById("cameraPreviousInput"),
  cameraNextInput: document.getElementById("cameraNextInput"),
  cameraStatusInput: document.getElementById("cameraStatusInput"),
  resetCameraFormBtn: document.getElementById("resetCameraFormBtn"),
  catalogCount: document.getElementById("catalogCount"),
  catalogTable: document.getElementById("catalogTable"),
  routeFocus: document.getElementById("routeFocus"),
  routeCandidates: document.getElementById("routeCandidates"),
  routeCount: document.getElementById("routeCount"),
  routeList: document.getElementById("routeList"),
  mapAreaInput: document.getElementById("mapAreaInput"),
  mapCameraInput: document.getElementById("mapCameraInput"),
  mapXInput: document.getElementById("mapXInput"),
  mapYInput: document.getElementById("mapYInput"),
  mapFacingInput: document.getElementById("mapFacingInput"),
  saveMapPositionBtn: document.getElementById("saveMapPositionBtn"),
  autoPlaceMapBtn: document.getElementById("autoPlaceMapBtn"),
  mapCameraDetail: document.getElementById("mapCameraDetail"),
  mapCount: document.getElementById("mapCount"),
  siteMap: document.getElementById("siteMap"),
  pbTreeSearchInput: document.getElementById("pbTreeSearchInput"),
  pbCameraList: document.getElementById("pbCameraList"),
  pbCameraCount: document.getElementById("pbCameraCount"),
  pbResourceTabs: document.querySelectorAll("[data-pb-resource-tab]"),
  pbResourcePane: document.getElementById("pbResourcePane"),
  pbAutoSwitchPane: document.getElementById("pbAutoSwitchPane"),
  playbackPanel: document.getElementById("playbackPanel"),
  playbackWorkspaceTabs: document.getElementById("playbackWorkspaceTabs"),
  playbackCanvasWrap: document.getElementById("playbackCanvasWrap"),
  playbackSpatialCanvasEl: document.getElementById("playbackSpatialCanvas"),
  playbackTileGrid: document.getElementById("playbackTileGrid"),
  pbLayoutBtn: document.getElementById("pbLayoutBtn"),
  pbLayoutPopover: document.getElementById("pbLayoutPopover"),
  pbDivisionCustomInput: document.getElementById("pbDivisionCustomInput"),
  pbDivisionCustomBtn: document.getElementById("pbDivisionCustomBtn"),
  pbCloseAllBtn: document.getElementById("pbCloseAllBtn"),
  pbFullscreenBtn: document.getElementById("pbFullscreenBtn"),
  pbRangeBtn: document.getElementById("pbRangeBtn"),
  pbRangeLabel: document.getElementById("pbRangeLabel"),
  pbRangePopover: document.getElementById("pbRangePopover"),
  pbRangeStartDate: document.getElementById("pbRangeStartDate"),
  pbRangeStartTime: document.getElementById("pbRangeStartTime"),
  pbRangeEndDate: document.getElementById("pbRangeEndDate"),
  pbRangeEndTime: document.getElementById("pbRangeEndTime"),
  pbRangeApplyBtn: document.getElementById("pbRangeApplyBtn"),
  pbScrubber: document.getElementById("pbScrubber"),
  pbJumpStartBtn: document.getElementById("pbJumpStartBtn"),
  pbStepBackBtn: document.getElementById("pbStepBackBtn"),
  pbPlayBtn: document.getElementById("pbPlayBtn"),
  pbStepForwardBtn: document.getElementById("pbStepForwardBtn"),
  pbJumpEndBtn: document.getElementById("pbJumpEndBtn"),
  pbCurrentTime: document.getElementById("pbCurrentTime"),
  pbSpeedBtn: document.getElementById("pbSpeedBtn"),
  pbFilterHuman: document.getElementById("pbFilterHuman"),
  pbFilterVehicle: document.getElementById("pbFilterVehicle"),
  pbTrack: document.getElementById("pbTrack"),
  pbTrackAvailable: document.getElementById("pbTrackAvailable"),
  pbTicks: document.getElementById("pbTicks"),
  pbPlayhead: document.getElementById("pbPlayhead"),
  pbPrevDayBtn: document.getElementById("pbPrevDayBtn"),
  pbNextDayBtn: document.getElementById("pbNextDayBtn"),
  pbDateLabel: document.getElementById("pbDateLabel"),
  incidentStatusFilter: document.getElementById("incidentStatusFilter"),
  incidentSearchInput: document.getElementById("incidentSearchInput"),
  incidentStats: document.getElementById("incidentStats"),
  incidentPreview: document.getElementById("incidentPreview"),
  incidentCount: document.getElementById("incidentCount"),
  incidentList: document.getElementById("incidentList"),
  groupForm: document.getElementById("groupForm"),
  groupIdInput: document.getElementById("groupIdInput"),
  groupNameInput: document.getElementById("groupNameInput"),
  groupPurposeInput: document.getElementById("groupPurposeInput"),
  groupGridInput: document.getElementById("groupGridInput"),
  groupCameraSelect: document.getElementById("groupCameraSelect"),
  groupDeviceFilterInput: document.getElementById("groupDeviceFilterInput"),
  importSelectedCamerasBtn: document.getElementById("importSelectedCamerasBtn"),
  importAllDeviceCamerasBtn: document.getElementById("importAllDeviceCamerasBtn"),
  groupMembersList: document.getElementById("groupMembersList"),
  groupNotesInput: document.getElementById("groupNotesInput"),
  resetGroupFormBtn: document.getElementById("resetGroupFormBtn"),
  groupCount: document.getElementById("groupCount"),
  groupList: document.getElementById("groupList"),
  userForm: document.getElementById("userForm"),
  userIdInput: document.getElementById("userIdInput"),
  userNameInput: document.getElementById("userNameInput"),
  usernameInput: document.getElementById("usernameInput"),
  userRoleInput: document.getElementById("userRoleInput"),
  userStatusInput: document.getElementById("userStatusInput"),
  userGroupSelect: document.getElementById("userGroupSelect"),
  userPermissionSelect: document.getElementById("userPermissionSelect"),
  userNotesInput: document.getElementById("userNotesInput"),
  resetUserFormBtn: document.getElementById("resetUserFormBtn"),
  userCount: document.getElementById("userCount"),
  userList: document.getElementById("userList")
};

function workspaceList(kind) {
  return kind === "live" ? state.liveWorkspaces : state.playbackWorkspaces;
}

function activeWorkspace(kind) {
  const id = kind === "live" ? state.activeLiveWorkspaceId : state.activePlaybackWorkspaceId;
  return workspaceList(kind).find((workspace) => workspace.id === id) || workspaceList(kind)[0];
}

function syncActiveWorkspaceFromLegacy(kind) {
  const workspace = activeWorkspace(kind);
  if (!workspace) return;
  if (kind === "live") {
    workspace.openIds = [...state.openIds];
    workspace.selectedId = state.selectedId;
    workspace.gridDivision = state.gridDivision;
    return;
  }
  workspace.openIds = [...state.playbackOpenIds];
  workspace.selectedId = state.playbackSelectedId;
  workspace.gridDivision = state.playbackGridDivision;
  workspace.rangeStart = state.playbackRangeStart;
  workspace.rangeEnd = state.playbackRangeEnd;
}

function syncAllActiveWorkspacesFromLegacy() {
  syncActiveWorkspaceFromLegacy("live");
  syncActiveWorkspaceFromLegacy("playback");
}

function loadWorkspaceIntoLegacy(kind) {
  const workspace = activeWorkspace(kind);
  if (!workspace) return;
  if (kind === "live") {
    state.openIds = [...workspace.openIds];
    state.selectedId = workspace.selectedId || state.cameras[0]?.id || "";
    state.gridDivision = workspace.gridDivision;
    return;
  }
  state.playbackOpenIds = [...workspace.openIds];
  state.playbackSelectedId = workspace.selectedId || "";
  state.playbackGridDivision = workspace.gridDivision;
  state.playbackRangeStart = workspace.rangeStart || `${todayValue()} 00:00:00`;
  state.playbackRangeEnd = workspace.rangeEnd || `${todayValue()} 23:59:59`;
}

function autoResourceLimit() {
  const cores = Math.max(2, Number(globalThis.navigator?.hardwareConcurrency) || 4);
  const memoryGb = Math.max(2, Number(globalThis.navigator?.deviceMemory) || 4);
  const cpuAllowance = cores * 2;
  const memoryAllowance = memoryGb * 2;
  return Math.max(4, Math.min(24, Math.round(Math.min(cpuAllowance, memoryAllowance))));
}

function effectiveResourceLimit() {
  return state.resourceOptimizationMode === "manual"
    ? Math.max(1, Math.min(64, Number(state.maxActiveStreams) || 6))
    : autoResourceLimit();
}

function totalOpenCameraSessions() {
  syncAllActiveWorkspacesFromLegacy();
  return [...state.liveWorkspaces, ...state.playbackWorkspaces]
    .reduce((total, workspace) => total + workspace.openIds.length, 0);
}

function workspaceAdmissionIds(kind, requestedIds) {
  syncAllActiveWorkspacesFromLegacy();
  const workspace = activeWorkspace(kind);
  if (!workspace) return [];
  const uniqueRequested = [...new Set(requestedIds.filter((id) => byId.has(id)))];
  const totalWithoutCurrent = totalOpenCameraSessions() - workspace.openIds.length;
  const availableForWorkspace = Math.max(0, effectiveResourceLimit() - totalWithoutCurrent);
  const admitted = uniqueRequested.slice(0, availableForWorkspace);
  if (admitted.length < uniqueRequested.length) {
    state.resourceLimitNotice = `${uniqueRequested.length - admitted.length} camera session${uniqueRequested.length - admitted.length === 1 ? "" : "s"} held back by the shared resource limit.`;
  } else {
    state.resourceLimitNotice = "";
  }
  return admitted;
}

function canAdmitCamera(kind, cameraId) {
  syncAllActiveWorkspacesFromLegacy();
  const workspace = activeWorkspace(kind);
  if (!workspace || workspace.openIds.includes(cameraId)) return true;
  if (totalOpenCameraSessions() < effectiveResourceLimit()) {
    state.resourceLimitNotice = "";
    return true;
  }
  state.resourceLimitNotice = "Shared camera-session limit reached. Close a tile, close a workspace tab, or increase the manual limit.";
  return false;
}

function enforceGlobalCameraLimit() {
  syncAllActiveWorkspacesFromLegacy();
  const limit = effectiveResourceLimit();
  const primaryKind = state.view === "playback" ? "playback" : "live";
  const primary = activeWorkspace(primaryKind);
  const secondary = activeWorkspace(primaryKind === "live" ? "playback" : "live");
  const ordered = [
    primary,
    secondary,
    ...state.liveWorkspaces,
    ...state.playbackWorkspaces
  ].filter((workspace, index, items) => workspace && items.indexOf(workspace) === index);
  let remaining = limit;
  let removed = 0;
  ordered.forEach((workspace) => {
    const kept = workspace.openIds.slice(0, remaining);
    removed += workspace.openIds.length - kept.length;
    workspace.openIds = kept;
    remaining = Math.max(0, remaining - kept.length);
    if (workspace.selectedId && !kept.includes(workspace.selectedId)) {
      workspace.selectedId = kept[0] || "";
    }
  });
  loadWorkspaceIntoLegacy("live");
  loadWorkspaceIntoLegacy("playback");
  state.resourceLimitNotice = removed
    ? `${removed} camera session${removed === 1 ? "" : "s"} closed to satisfy the shared resource limit.`
    : "";
}

function resetPlaybackTimelineForWorkspace() {
  const start = parseStoredDateTime(state.playbackRangeStart);
  playbackActiveDay = startOfDay(start);
  playbackCursorDate = clampDateToDay(start, playbackActiveDay);
  stopPlaybackPlay();
}

function nextWorkspaceId(kind) {
  const prefix = kind === "playback" ? "playback" : "live";
  let id = "";
  do {
    workspaceIdSequence += 1;
    id = `${prefix}-${Date.now().toString(36)}-${workspaceIdSequence.toString(36)}`;
  } while ([...state.liveWorkspaces, ...state.playbackWorkspaces].some((workspace) => workspace.id === id));
  return id;
}

function activateWorkspace(kind, id) {
  if (!workspaceList(kind).some((workspace) => workspace.id === id)) return;
  syncActiveWorkspaceFromLegacy(kind);
  if (kind === "live") {
    state.activeLiveWorkspaceId = id;
    maximizedLiveCameraId = "";
  } else {
    state.activePlaybackWorkspaceId = id;
    maximizedPlaybackCameraId = "";
  }
  loadWorkspaceIntoLegacy(kind);
  if (kind === "playback") resetPlaybackTimelineForWorkspace();
  persist();
  render();
}

function createWorkspace(kind) {
  syncActiveWorkspaceFromLegacy(kind);
  const workspaces = workspaceList(kind);
  const index = workspaces.length + 1;
  const id = nextWorkspaceId(kind);
  const workspace = {
    id,
    name: `${kind === "live" ? "Live" : "Playback"} ${index}`,
    openIds: [],
    selectedId: "",
    gridDivision: kind === "live" ? 9 : 4,
    rangeStart: kind === "playback" ? `${todayValue()} 00:00:00` : undefined,
    rangeEnd: kind === "playback" ? `${todayValue()} 23:59:59` : undefined,
    floating: false,
    layer: "front",
    floatRect: null
  };
  workspaces.push(workspace);
  if (kind === "live") state.activeLiveWorkspaceId = id;
  else state.activePlaybackWorkspaceId = id;
  loadWorkspaceIntoLegacy(kind);
  if (kind === "playback") resetPlaybackTimelineForWorkspace();
  persist();
  render();
}

function closeWorkspace(kind, id) {
  const workspaces = workspaceList(kind);
  if (workspaces.length <= 1) return;
  syncActiveWorkspaceFromLegacy(kind);
  const index = workspaces.findIndex((workspace) => workspace.id === id);
  if (index < 0) return;
  const wasActive = activeWorkspace(kind)?.id === id;
  workspaces.splice(index, 1);
  if (wasActive) {
    const replacement = workspaces[Math.min(index, workspaces.length - 1)];
    if (kind === "live") {
      state.activeLiveWorkspaceId = replacement.id;
      maximizedLiveCameraId = "";
    } else {
      state.activePlaybackWorkspaceId = replacement.id;
      maximizedPlaybackCameraId = "";
    }
    loadWorkspaceIntoLegacy(kind);
    if (kind === "playback") resetPlaybackTimelineForWorkspace();
  }
  persist();
  render();
}

function toggleWorkspaceFloating(kind) {
  syncActiveWorkspaceFromLegacy(kind);
  const workspace = activeWorkspace(kind);
  if (!workspace) return;
  workspace.floating = !workspace.floating;
  workspace.layer = "front";
  if (workspace.floating) {
    const otherKind = kind === "live" ? "playback" : "live";
    const otherWorkspace = activeWorkspace(otherKind);
    if (otherWorkspace?.floating) otherWorkspace.layer = "back";
  }
  persist();
  setView(state.view);
  renderWorkspaceTabs(kind);
  renderWorkspaceTabs(kind === "live" ? "playback" : "live");
  applyWorkspacePresentation();
}

function toggleWorkspaceLayer(kind) {
  const workspace = activeWorkspace(kind);
  if (!workspace || !workspace.floating) return;
  workspace.layer = workspace.layer === "front" ? "back" : "front";
  if (workspace.layer === "front") {
    const otherWorkspace = activeWorkspace(kind === "live" ? "playback" : "live");
    if (otherWorkspace?.floating) otherWorkspace.layer = "back";
  }
  persist();
  renderWorkspaceTabs(kind);
  renderWorkspaceTabs(kind === "live" ? "playback" : "live");
  applyWorkspacePresentation();
}

function defaultWorkspaceFloatRect(kind) {
  const viewportWidth = Math.max(760, Number(window.innerWidth) || 1280);
  const viewportHeight = Math.max(560, Number(window.innerHeight) || 800);
  const width = Math.min(1120, Math.max(620, Math.round(viewportWidth * 0.72)));
  const height = Math.min(760, Math.max(420, Math.round(viewportHeight * 0.72)));
  const availableX = Math.max(12, viewportWidth - width - 12);
  const availableY = Math.max(58, viewportHeight - height - 12);
  return {
    x: Math.min(availableX, kind === "live" ? 86 : 150),
    y: Math.min(availableY, kind === "live" ? 72 : 96),
    width,
    height
  };
}

function constrainWorkspaceFloatRect(kind, rect) {
  const viewportWidth = Math.max(420, Number(window.innerWidth) || 1280);
  const viewportHeight = Math.max(360, Number(window.innerHeight) || 800);
  const fallback = defaultWorkspaceFloatRect(kind);
  const width = Math.min(
    Math.max(360, viewportWidth - 24),
    Math.max(360, Number(rect?.width) || fallback.width)
  );
  const height = Math.min(
    Math.max(280, viewportHeight - 24),
    Math.max(280, Number(rect?.height) || fallback.height)
  );
  return {
    x: Math.max(8, Math.min(viewportWidth - width - 8, Number(rect?.x) || fallback.x)),
    y: Math.max(8, Math.min(viewportHeight - height - 8, Number(rect?.y) || fallback.y)),
    width,
    height
  };
}

function startWorkspaceDrag(kind, event, handle) {
  const workspace = activeWorkspace(kind);
  const view = document.getElementById(kind === "live" ? "operatorView" : "playbackView");
  if (!workspace?.floating || !view || event.button !== 0) return;
  event.preventDefault();
  const box = view.getBoundingClientRect();
  const origin = {
    pointerX: event.clientX,
    pointerY: event.clientY,
    rect: { x: box.left, y: box.top, width: box.width, height: box.height }
  };
  workspace.floatRect = origin.rect;
  handle.setPointerCapture?.(event.pointerId);

  const move = (moveEvent) => {
    if (moveEvent.pointerId !== event.pointerId) return;
    workspace.floatRect = constrainWorkspaceFloatRect(kind, {
      ...origin.rect,
      x: origin.rect.x + moveEvent.clientX - origin.pointerX,
      y: origin.rect.y + moveEvent.clientY - origin.pointerY
    });
    applyWorkspacePresentation();
  };
  const finish = (finishEvent) => {
    if (finishEvent.pointerId !== event.pointerId) return;
    handle.removeEventListener("pointermove", move);
    handle.removeEventListener("pointerup", finish);
    handle.removeEventListener("pointercancel", finish);
    handle.releasePointerCapture?.(event.pointerId);
    persist();
  };
  handle.addEventListener("pointermove", move);
  handle.addEventListener("pointerup", finish);
  handle.addEventListener("pointercancel", finish);
}

function applyWorkspacePresentation() {
  const liveView = document.getElementById("operatorView");
  const playbackView = document.getElementById("playbackView");
  [
    [liveView, activeWorkspace("live")],
    [playbackView, activeWorkspace("playback")]
  ].forEach(([view, workspace]) => {
    if (!view || !workspace) return;
    view.classList.toggle("workspace-floating", workspace.floating);
    view.classList.toggle("workspace-layer-front", workspace.floating && workspace.layer === "front");
    view.classList.toggle("workspace-layer-back", workspace.floating && workspace.layer === "back");
    if (workspace.floating) {
      const kind = view === liveView ? "live" : "playback";
      workspace.floatRect = constrainWorkspaceFloatRect(kind, workspace.floatRect);
      view.style.left = `${workspace.floatRect.x}px`;
      view.style.top = `${workspace.floatRect.y}px`;
      view.style.width = `${workspace.floatRect.width}px`;
      view.style.height = `${workspace.floatRect.height}px`;
    } else {
      view.style.removeProperty("left");
      view.style.removeProperty("top");
      view.style.removeProperty("width");
      view.style.removeProperty("height");
    }
  });
}

function renderWorkspaceTabs(kind) {
  const container = kind === "live" ? els.liveWorkspaceTabs : els.playbackWorkspaceTabs;
  if (!container) return;
  syncActiveWorkspaceFromLegacy(kind);
  const workspaces = workspaceList(kind);
  const active = activeWorkspace(kind);
  const used = totalOpenCameraSessions();
  const limit = effectiveResourceLimit();
  const tabs = workspaces.map((workspace) => `
    <div class="workspace-tab ${workspace.id === active?.id ? "active" : ""}">
      <button class="workspace-tab-main" data-activate-workspace="${escapeHtml(workspace.id)}" type="button" role="tab" aria-selected="${workspace.id === active?.id}">
        <span>${escapeHtml(workspace.name)}</span>
        <small>${workspace.openIds.length}</small>
        ${workspace.floating ? `<span class="workspace-float-mark" title="Floating workspace">&#8599;</span>` : ""}
      </button>
      <button class="workspace-tab-close" data-close-workspace="${escapeHtml(workspace.id)}" type="button" aria-label="Close ${escapeHtml(workspace.name)}" ${workspaces.length <= 1 ? "disabled" : ""}>&times;</button>
    </div>
  `).join("");
  container.innerHTML = `
    <div class="workspace-tab-list" role="tablist" aria-label="${kind === "live" ? "Live View" : "Remote Playback"} workspaces">${tabs}</div>
    <div class="workspace-tab-actions">
      <span class="workspace-drag-handle" data-workspace-drag title="Drag floating workspace" aria-hidden="true">&#8942;&#8942;</span>
      <span class="workspace-resource-meter ${used >= limit ? "at-limit" : ""}" title="Shared camera sessions across all workspaces">${used}/${limit}</span>
      <button data-workspace-command="new" type="button" title="New ${kind === "live" ? "Live View" : "Playback"} tab" aria-label="New ${kind === "live" ? "Live View" : "Playback"} tab">+</button>
      <button data-workspace-command="float" type="button" title="${active?.floating ? "Dock workspace" : "Float workspace"}">${active?.floating ? "Dock" : "Float"}</button>
      <button data-workspace-command="layer" type="button" title="Move floating workspace to the ${active?.layer === "front" ? "back" : "front"}" ${active?.floating ? "" : "disabled"}>${active?.layer === "front" ? "Back" : "Front"}</button>
    </div>
  `;
  container.onclick = (event) => {
    const activateButton = event.target.closest("[data-activate-workspace]");
    if (activateButton) {
      activateWorkspace(kind, activateButton.dataset.activateWorkspace);
      return;
    }
    const closeButton = event.target.closest("[data-close-workspace]");
    if (closeButton) {
      event.stopPropagation();
      closeWorkspace(kind, closeButton.dataset.closeWorkspace);
      return;
    }
    const command = event.target.closest("[data-workspace-command]")?.dataset.workspaceCommand;
    if (command === "new") createWorkspace(kind);
    if (command === "float") toggleWorkspaceFloating(kind);
    if (command === "layer") toggleWorkspaceLayer(kind);
  };
  container.onpointerdown = (event) => {
    const handle = event.target.closest("[data-workspace-drag]");
    if (handle) startWorkspaceDrag(kind, event, handle);
  };
}

function refreshWorkspaceResourceUi() {
  renderWorkspaceTabs("live");
  renderWorkspaceTabs("playback");
  syncStreamingControlState();
}

function persist() {
  syncAllActiveWorkspacesFromLegacy();
  localStorage.setItem("selectedCamera", state.selectedId);
  localStorage.setItem("openCameraIds", JSON.stringify(state.openIds));
  localStorage.setItem("devices", JSON.stringify(state.devices));
  localStorage.setItem("cameras", JSON.stringify(state.cameras));
  localStorage.setItem("activeView", state.view);
  localStorage.setItem("theme", state.theme);
  localStorage.setItem("routeSelectedCamera", state.routeSelectedId);
  localStorage.setItem("mapSelectedCamera", state.mapSelectedId);
  localStorage.setItem("mapArea", state.mapArea);
  localStorage.setItem("mapPositions", JSON.stringify(state.mapPositions));
  localStorage.setItem("playbackOpenIds", JSON.stringify(state.playbackOpenIds));
  localStorage.setItem("playbackGridDivision", String(state.playbackGridDivision));
  localStorage.setItem("playbackSelectedCamera", state.playbackSelectedId);
  localStorage.setItem("playbackRangeStart", state.playbackRangeStart);
  localStorage.setItem("playbackRangeEnd", state.playbackRangeEnd);
  localStorage.setItem("incidents", JSON.stringify(state.incidents));
  localStorage.setItem("selectedIncident", state.selectedIncidentId);
  localStorage.setItem("cameraGroups", JSON.stringify(state.groups));
  localStorage.setItem("selectedGroup", state.selectedGroupId);
  localStorage.setItem("users", JSON.stringify(state.users));
  localStorage.setItem("selectedUser", state.selectedUserId);
  localStorage.setItem("showMediaPolicy", String(state.showMediaPolicy));
  localStorage.setItem("useDraggableCanvases", String(state.useDraggableCanvases));
  localStorage.setItem("restoreLastCameraGrid", String(state.restoreLastCameraGrid));
  localStorage.setItem("liveWorkspaces", JSON.stringify(state.liveWorkspaces));
  localStorage.setItem("playbackWorkspaces", JSON.stringify(state.playbackWorkspaces));
  localStorage.setItem("activeLiveWorkspaceId", state.activeLiveWorkspaceId);
  localStorage.setItem("activePlaybackWorkspaceId", state.activePlaybackWorkspaceId);
  localStorage.setItem("resourceOptimizationMode", state.resourceOptimizationMode);
  localStorage.setItem("liveViewport", JSON.stringify(state.liveViewport));
  localStorage.setItem("maxActiveStreams", String(state.maxActiveStreams));
  localStorage.setItem("gridDivision", String(state.gridDivision));
  localStorage.setItem("liveDigitalZoom", String(state.liveDigitalZoom));
  queueInventorySync();
}

function escapeHtml(value) {
  return String(value ?? "").replace(/[&<>"']/g, (char) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    "\"": "&quot;",
    "'": "&#39;"
  }[char]));
}

function clampNumber(value, min, max, fallback) {
  const parsed = Number(value);
  if (!Number.isFinite(parsed)) return fallback;
  return Math.max(min, Math.min(max, parsed));
}

function applySidebarState() {
  if (localStorage.getItem("sidebarDefaultCollapsedV2") !== "true") {
    localStorage.setItem("sidebarCollapsed", "true");
    localStorage.setItem("sidebarDefaultCollapsedV2", "true");
  }
  const saved = localStorage.getItem("sidebarCollapsed");
  const collapsed = saved === null ? true : saved === "true";
  els.appShell.classList.toggle("sidebar-collapsed", collapsed);
  if (els.sidebarToggle) {
    const label = collapsed ? "Expand menu" : "Collapse menu";
    els.sidebarToggle.title = label;
    els.sidebarToggle.setAttribute("aria-label", label);
  }
}

function refreshCameraIndexes() {
  syncAllActiveWorkspacesFromLegacy();
  migrateCameraDeviceLinks();
  byId = new Map(state.cameras.map((camera) => [camera.id, camera]));
  ensureDeviceGroups();
  areas = ["All", ...new Set(state.cameras.map((camera) => camera.area).filter(Boolean))];
  tags = ["All", ...new Set(state.cameras.flatMap((camera) => camera.tags || []))].sort((a, b) => {
    if (a === "All") return -1;
    if (b === "All") return 1;
    return a.localeCompare(b);
  });

  [...state.liveWorkspaces, ...state.playbackWorkspaces].forEach((workspace) => {
    workspace.openIds = workspace.openIds.filter((id) => byId.has(id));
    if (workspace.selectedId && !byId.has(workspace.selectedId)) {
      workspace.selectedId = workspace.openIds[0] || "";
    }
  });
  loadWorkspaceIntoLegacy("live");
  loadWorkspaceIntoLegacy("playback");

  if (!byId.has(state.selectedId) && state.cameras.length) {
    state.selectedId = state.cameras[0].id;
  }
  if (!byId.has(state.routeSelectedId) && state.cameras.length) {
    state.routeSelectedId = state.selectedId || state.cameras[0].id;
  }
  if (!byId.has(state.mapSelectedId) && state.cameras.length) {
    state.mapSelectedId = state.selectedId || state.cameras[0].id;
  }
  if (!state.cameras.length) {
    state.selectedId = "";
    state.routeSelectedId = "";
    state.mapSelectedId = "";
  }
  state.openIds = state.openIds.filter((id) => byId.has(id));
  state.playbackOpenIds = state.playbackOpenIds.filter((id) => byId.has(id));
  state.groups = state.groups.map((group) => ({ ...group, cameraIds: (group.cameraIds || []).filter((id) => byId.has(id)) }));
  if (state.selectedGroupId && !state.groups.some((group) => group.id === state.selectedGroupId)) {
    state.selectedGroupId = "";
  }
  if (!state.selectedGroupId && state.groups.length) {
    state.selectedGroupId = state.groups[0].id;
  }
  state.users = state.users.map((user) => ({ ...user, groupIds: (user.groupIds || []).filter((id) => state.groups.some((group) => group.id === id)) }));
  syncAllActiveWorkspacesFromLegacy();
}

function csvToList(value) {
  return value
    .split(",")
    .map((item) => item.trim())
    .filter(Boolean);
}

function slug(value) {
  return value
    .toUpperCase()
    .replace(/[^A-Z0-9]+/g, "-")
    .replace(/^-|-$/g, "")
    .slice(0, 24);
}

function isIpCameraDirectType(type) {
  return String(type || "").trim().toLowerCase() === "ip camera direct";
}

function deviceChannelCount(device) {
  if (isIpCameraDirectType(device?.type)) return 1;
  return Math.round(clampNumber(device?.channels, 1, 256, 1));
}

function cameraBelongsToDevice(camera, device) {
  if (!camera || !device) return false;
  if (camera.deviceId) return camera.deviceId === device.id;
  return camera.nvr === device.name;
}

function cameraDevice(camera) {
  if (!camera) return undefined;
  return (
    state.devices.find((device) => device.id === camera.deviceId) ||
    state.devices.find((device) => device.name === camera.nvr)
  );
}

function cameraDeviceName(camera) {
  return cameraDevice(camera)?.name || camera?.nvr || "Unassigned";
}

function migrateCameraDeviceLinks() {
  const devicesById = new Map(state.devices.map((device) => [device.id, device]));
  const devicesByName = new Map();
  state.devices.forEach((device) => {
    if (!devicesByName.has(device.name)) devicesByName.set(device.name, device);
  });

  let changed = false;
  state.cameras = state.cameras.map((camera) => {
    const device = devicesById.get(camera.deviceId) || devicesByName.get(camera.nvr);
    const deviceId = device?.id || "";
    const nvr = device?.name || camera.nvr || "Unassigned";
    if (camera.deviceId === deviceId && camera.nvr === nvr) return camera;
    changed = true;
    return { ...camera, deviceId, nvr };
  });

  pendingCameraLinkMigration = pendingCameraLinkMigration || changed;
  return changed;
}

function inventoryApiAvailable() {
  const protocol = window?.location?.protocol || "";
  return !isVmsTestMode && protocol !== "file:" && typeof fetch === "function";
}

function inventoryMutationAllowed({
  apiAvailable = inventoryApiAvailable(),
  authorityState = inventoryAuthorityState
} = {}) {
  return !apiAvailable || authorityState === "connected";
}

function inventoryAuthorityMessage() {
  const lastSaved = inventoryLastSuccessfulAt
    ? ` Last confirmed ${new Date(inventoryLastSuccessfulAt).toLocaleString()}.`
    : "";

  switch (inventoryAuthorityState) {
    case "local_only":
      return "Local-only prototype: inventory changes stay in this browser.";
    case "checking":
      return inventoryAuthorityDetail || "Checking the local inventory service...";
    case "connected":
      return `Local inventory service connected.${lastSaved}`;
    case "saving":
      return "Saving inventory to the local service...";
    case "save_failed":
      return inventoryAuthorityDetail
        || "Inventory save failed. The unsaved local draft was preserved and editing is paused.";
    case "read_only":
      return inventoryAuthorityDetail
        || "Inventory service unavailable. Cached inventory is visible but read-only.";
    default:
      return "Inventory availability is unknown.";
  }
}

function updateInventoryMutationControls() {
  const disabled = !inventoryMutationAllowed();
  [els.deviceForm, els.cameraForm, els.groupForm].forEach((form) => {
    if (!form || typeof form.querySelectorAll !== "function") return;
    form.querySelectorAll('button[type="submit"], button[type="reset"]').forEach((button) => {
      button.disabled = disabled;
    });
  });

  [
    els.deviceAddBtn,
    els.deviceDeleteBtn,
    els.resetDeviceFormBtn,
    els.resetCameraFormBtn,
    els.createLiveGroupBtn,
    els.addCheckedToGroupBtn,
    els.importSelectedCamerasBtn,
    els.importAllDeviceCamerasBtn,
    els.resetGroupFormBtn
  ].forEach((control) => {
    if (control) control.disabled = disabled;
  });

  document
    .querySelectorAll(
      "[data-sync-device], [data-delete-device], [data-delete-camera], "
      + "[data-add-route], [data-remove-route], [data-delete-group]"
    )
    .forEach((control) => {
      control.disabled = disabled;
    });
}

function renderInventoryAuthorityStatus() {
  if (els.statusMessage) {
    els.statusMessage.textContent = inventoryAuthorityMessage();
    els.statusMessage.dataset.inventoryState = inventoryAuthorityState;
  }
  if (els.inventoryRetryBtn) {
    const canRecheck = inventoryApiAvailable()
      && (inventoryAuthorityState === "read_only" || inventoryAuthorityState === "save_failed");
    els.inventoryRetryBtn.hidden = !canRecheck;
    els.inventoryRetryBtn.textContent = inventoryPendingSnapshot ? "Retry saved draft" : "Recheck";
  }
  updateInventoryMutationControls();
}

function setInventoryAuthorityState(nextState, detail = "") {
  inventoryAuthorityState = nextState;
  inventoryAuthorityDetail = detail;
  renderInventoryAuthorityStatus();
}

function requireInventoryMutation() {
  if (inventoryMutationAllowed()) return true;
  const message = inventoryAuthorityState === "saving"
    ? "Wait for the current inventory save to finish."
    : "Inventory editing is paused because the inventory service is not available for confirmed saves.";
  window.alert(message);
  return false;
}

function createInventorySnapshot() {
  return {
    version: 2,
    devices: state.devices.map((device) => {
      const { password: _password, ...safeDevice } = device;
      const channels = deviceChannelCount(device);
      return { ...safeDevice, channels, channelCount: channels };
    }),
    cameras: state.cameras.map((camera) => ({ ...camera })),
    groups: state.groups.map((group) => ({
      ...group,
      cameraIds: [...(group.cameraIds || [])]
    }))
  };
}

function normalizeRemoteDevice(device, localDevice) {
  const { password: _remotePassword, ...safeDevice } = device || {};
  const type = safeDevice.type || "NVR";
  const channels = deviceChannelCount({
    type,
    channels: safeDevice.channels ?? safeDevice.channelCount ?? 1
  });
  return {
    ...safeDevice,
    id: String(safeDevice.id || ""),
    name: String(safeDevice.name || safeDevice.host || "Unnamed device"),
    type,
    vendor: safeDevice.vendor || "",
    host: safeDevice.host || "",
    port: Number(safeDevice.port || 0),
    channels,
    channelCount: channels,
    status: safeDevice.status || "unknown",
    username: safeDevice.username ?? localDevice?.username ?? "",
    password: localDevice?.password || "",
    notes: safeDevice.notes || ""
  };
}

function normalizeRemoteCamera(camera, devicesById) {
  const deviceId = String(camera?.deviceId || "");
  const device = devicesById.get(deviceId);
  const channel = Number(camera?.channel ?? camera?.channelNumber ?? 1);
  const tags = Array.isArray(camera?.tags) ? camera.tags : [];
  const inferredPlaceholder =
    camera?.deviceSyncManaged === true &&
    camera?.discovered === true &&
    tags.includes("unmapped") &&
    (camera?.area || "Unassigned") === "Unassigned";
  return {
    ...(camera || {}),
    id: String(camera?.id || ""),
    name: String(camera?.name || camera?.displayName || `Camera CH-${String(channel).padStart(2, "0")}`),
    deviceId,
    nvr: device?.name || camera?.nvr || "Unassigned",
    channel,
    area: camera?.area || "Unassigned",
    floor: camera?.floor || "Unknown",
    direction: camera?.direction || "Direction not set",
    status: camera?.status || "unknown",
    tags,
    related: Array.isArray(camera?.related) ? camera.related : [],
    previous: Array.isArray(camera?.previous) ? camera.previous : [],
    next: Array.isArray(camera?.next) ? camera.next : [],
    managedPlaceholder: camera?.managedPlaceholder ?? inferredPlaceholder
  };
}

function normalizeRemoteGroup(group) {
  return {
    ...(group || {}),
    id: String(group?.id || ""),
    name: String(group?.name || "Unnamed group"),
    purpose: group?.purpose || "",
    grid: Number(group?.grid ?? group?.preferredGrid ?? group?.preferred_grid ?? 4),
    cameraIds: Array.isArray(group?.cameraIds) ? [...group.cameraIds] : [],
    notes: group?.notes || "",
    system: Boolean(group?.system ?? group?.isSystem ?? group?.is_system),
    deviceId: group?.deviceId || group?.device_id || ""
  };
}

function normalizeInventoryPayload(payload) {
  const snapshot = payload?.inventory || payload;
  if (!snapshot || !Array.isArray(snapshot.devices) || !Array.isArray(snapshot.cameras) || !Array.isArray(snapshot.groups)) {
    throw new Error("The inventory service returned an invalid snapshot.");
  }

  const localDevicesById = new Map(state.devices.map((device) => [device.id, device]));
  const devices = snapshot.devices
    .map((device) => normalizeRemoteDevice(device, localDevicesById.get(device?.id)))
    .filter((device) => device.id);
  const devicesById = new Map(devices.map((device) => [device.id, device]));
  const cameras = snapshot.cameras
    .map((camera) => normalizeRemoteCamera(camera, devicesById))
    .filter((camera) => camera.id);
  const groups = snapshot.groups
    .map(normalizeRemoteGroup)
    .filter((group) => group.id);

  return { devices, cameras, groups };
}

function stableStringify(value) {
  if (Array.isArray(value)) {
    return `[${value.map(stableStringify).join(",")}]`;
  }
  if (value && typeof value === "object") {
    const keys = Object.keys(value).sort();
    return `{${keys.map((key) => `${JSON.stringify(key)}:${stableStringify(value[key])}`).join(",")}}`;
  }
  return JSON.stringify(value);
}

function inventoryFingerprint(payload) {
  const normalized = normalizeInventoryPayload(payload);
  const sortById = (left, right) => String(left.id).localeCompare(String(right.id));
  const canonical = {
    devices: normalized.devices.map((device) => ({
      id: device.id,
      name: device.name,
      type: device.type,
      vendor: device.vendor || "",
      host: device.host,
      port: Number(device.port || 0),
      channels: deviceChannelCount(device),
      status: device.status || "unknown",
      username: device.username || "",
      notes: device.notes || "",
      maxConcurrentMainstream: Number(device.maxConcurrentMainstream || 4),
      maxConcurrentSubstream: Number(device.maxConcurrentSubstream || 32)
    })).sort(sortById),
    cameras: normalized.cameras.map((camera) => ({
      id: camera.id,
      name: camera.name,
      deviceId: camera.deviceId || "",
      nvr: camera.nvr || "Unassigned",
      channel: Number(camera.channel || 1),
      stream: camera.stream || "",
      area: camera.area || "Unassigned",
      floor: camera.floor || "Unknown",
      direction: camera.direction || "Direction not set",
      status: camera.status || "unknown",
      tags: [...(camera.tags || [])],
      related: [...(camera.related || [])],
      previous: [...(camera.previous || [])],
      next: [...(camera.next || [])],
      discovered: Boolean(camera.discovered),
      managedPlaceholder: Boolean(camera.managedPlaceholder)
    })).sort(sortById),
    groups: normalized.groups.map((group) => ({
      id: group.id,
      name: group.name,
      purpose: group.purpose || "",
      grid: Number(group.grid || 4),
      cameraIds: [...(group.cameraIds || [])],
      notes: group.notes || "",
      system: Boolean(group.system),
      deviceId: group.deviceId || ""
    })).sort(sortById)
  };
  return stableStringify(canonical);
}

function applyInventorySnapshot(payload) {
  const normalized = normalizeInventoryPayload(payload);
  state.devices = normalized.devices;
  state.cameras = normalized.cameras;
  state.groups = normalized.groups;
  migrateCameraDeviceLinks();
  refreshCameraIndexes();
}

async function fetchInventoryApi(options = {}, timeoutMs = 2000) {
  const controller = typeof AbortController === "function" ? new AbortController() : null;
  const timeout = controller ? window.setTimeout(() => controller.abort(), timeoutMs) : 0;
  try {
    return await fetch("/api/inventory", {
      cache: "no-store",
      ...options,
      ...(controller ? { signal: controller.signal } : {})
    });
  } finally {
    if (timeout) window.clearTimeout(timeout);
  }
}

async function writeInventorySnapshot(serializedSnapshot) {
  const response = await fetchInventoryApi({
    method: "PUT",
    headers: { "Content-Type": "application/json", Accept: "application/json" },
    body: serializedSnapshot
  }, 5000);
  if (!response.ok) {
    const errorBody = await response.text().catch(() => "");
    throw new Error(errorBody || `Inventory save failed (${response.status}).`);
  }
  return response.json();
}

function acceptConfirmedInventory(remote) {
  applyInventorySnapshot(remote);
  pendingCameraLinkMigration = false;
  persist();

  lastInventoryFingerprint = inventoryFingerprint(createInventorySnapshot());
  inventoryLastSuccessfulAt = new Date().toISOString();
  inventoryPendingSnapshot = "";
  localStorage.setItem(INVENTORY_CONFIRMED_FINGERPRINT_KEY, lastInventoryFingerprint);
  localStorage.setItem(INVENTORY_LAST_SUCCESS_KEY, inventoryLastSuccessfulAt);
  localStorage.removeItem(INVENTORY_PENDING_SNAPSHOT_KEY);
  inventoryBackendReady = true;
  setInventoryAuthorityState("connected");
  render();
}

function preserveInventoryFailure(error, { saveFailed = Boolean(inventoryPendingSnapshot), detail = "" } = {}) {
  inventoryBackendReady = false;
  const errorMessage = error?.message ? ` ${error.message}` : "";
  const message = detail || (saveFailed
    ? `Inventory save was not confirmed. The unsaved local draft was preserved; automatic overwrite is blocked.${errorMessage}`
    : `Inventory service unavailable. Cached inventory is visible but read-only.${errorMessage}`);
  setInventoryAuthorityState(saveFailed ? "save_failed" : "read_only", message);
}

async function pushScheduledInventory(serializedSnapshot) {
  try {
    const saved = await writeInventorySnapshot(serializedSnapshot);
    acceptConfirmedInventory(saved);
  } catch (error) {
    preserveInventoryFailure(error, { saveFailed: true });
    console.warn("Inventory save was not confirmed; the unsaved local draft remains in the browser.", error);
  }
}

function queueInventorySync() {
  if (
    !inventoryBackendReady
    || !inventoryApiAvailable()
    || inventoryAuthorityState !== "connected"
  ) return;

  const snapshot = createInventorySnapshot();
  const serializedSnapshot = JSON.stringify(snapshot);
  const fingerprint = inventoryFingerprint(snapshot);
  if (fingerprint === lastInventoryFingerprint || fingerprint === scheduledInventoryFingerprint) return;

  if (inventorySyncTimer) window.clearTimeout(inventorySyncTimer);
  scheduledInventoryFingerprint = fingerprint;
  scheduledInventorySnapshot = serializedSnapshot;
  inventoryPendingSnapshot = serializedSnapshot;
  localStorage.setItem(INVENTORY_PENDING_SNAPSHOT_KEY, serializedSnapshot);
  setInventoryAuthorityState("saving");

  inventorySyncTimer = window.setTimeout(() => {
    const scheduledSnapshot = scheduledInventorySnapshot;
    scheduledInventoryFingerprint = "";
    scheduledInventorySnapshot = "";
    inventorySyncTimer = 0;
    inventorySyncChain = inventorySyncChain.then(() => pushScheduledInventory(scheduledSnapshot));
  }, 250);
}

async function initializeInventoryBackend() {
  if (!inventoryApiAvailable()) {
    setInventoryAuthorityState("local_only");
    return;
  }

  setInventoryAuthorityState("checking");
  try {
    const response = await fetchInventoryApi({ headers: { Accept: "application/json" } });
    if (!response.ok) throw new Error(`Inventory service unavailable (${response.status}).`);
    let remote = await response.json();

    if (inventoryPendingSnapshot) {
      let pending;
      try {
        pending = JSON.parse(inventoryPendingSnapshot);
      } catch {
        throw new Error("The preserved local inventory draft is not valid JSON.");
      }

      if (remote.initialized !== false && inventoryFingerprint(remote) === inventoryFingerprint(pending)) {
        acceptConfirmedInventory(remote);
        return;
      }

      preserveInventoryFailure(null, {
        saveFailed: true,
        detail: remote.initialized === false
          ? "An unsaved local inventory draft was preserved. The inventory service is empty; choose Retry saved draft to write it."
          : "An unsaved local inventory draft was preserved. It differs from the confirmed service copy, so automatic overwrite is blocked."
      });
      return;
    }

    if (remote.initialized === false) {
      remote = await writeInventorySnapshot(JSON.stringify(createInventorySnapshot()));
    }

    acceptConfirmedInventory(remote);
  } catch (error) {
    preserveInventoryFailure(error);
    console.info("Using a read-only local inventory cache; the inventory service is not available.", error);
  }
}

async function retryPendingInventorySave() {
  if (!inventoryApiAvailable()) return;
  if (!inventoryPendingSnapshot) {
    await initializeInventoryBackend();
    return;
  }

  setInventoryAuthorityState("checking", "Checking whether the preserved inventory draft can be saved safely...");
  try {
    const pending = JSON.parse(inventoryPendingSnapshot);
    const pendingFingerprint = inventoryFingerprint(pending);
    const response = await fetchInventoryApi({ headers: { Accept: "application/json" } });
    if (!response.ok) throw new Error(`Inventory service unavailable (${response.status}).`);
    const remote = await response.json();

    if (remote.initialized !== false) {
      const remoteFingerprint = inventoryFingerprint(remote);
      if (remoteFingerprint === pendingFingerprint) {
        acceptConfirmedInventory(remote);
        return;
      }
      if (!lastInventoryFingerprint || remoteFingerprint !== lastInventoryFingerprint) {
        preserveInventoryFailure(null, {
          saveFailed: true,
          detail: "The service inventory changed after this draft was created. The local draft was preserved, but overwrite is blocked until a later conflict-resolution workflow is approved."
        });
        return;
      }
    }

    setInventoryAuthorityState("saving", "Saving the preserved inventory draft...");
    const saved = await writeInventorySnapshot(inventoryPendingSnapshot);
    acceptConfirmedInventory(saved);
  } catch (error) {
    preserveInventoryFailure(error, { saveFailed: true });
  }
}

function channelCameraId(device, channel) {
  const channelSuffix = `CH${String(channel).padStart(2, "0")}`;
  const preferred = `${slug(device.name || device.host || "DEVICE")}-${channelSuffix}`;
  if (!state.cameras.some((camera) => camera.id === preferred)) return preferred;

  const stableBase = slug(device.id || device.host || "DEVICE");
  let candidate = `${stableBase}-${channelSuffix}`;
  let suffix = 2;
  while (state.cameras.some((camera) => camera.id === candidate)) {
    candidate = `${stableBase}-${channelSuffix}-${suffix}`;
    suffix += 1;
  }
  return candidate;
}

function deviceGroupId(device) {
  return `grp-device-${device.id}`;
}

function ensureDeviceGroups() {
  state.devices.forEach((device) => {
    const groupId = deviceGroupId(device);
    const cameraIds = state.cameras
      .filter((camera) => cameraBelongsToDevice(camera, device))
      .sort((a, b) => Number(a.channel || 0) - Number(b.channel || 0))
      .map((camera) => camera.id);
    const existing = state.groups.find((group) => group.id === groupId);

    if (existing) {
      existing.name = `${device.name} (Assigned)`;
      existing.purpose = existing.purpose || "Auto-assigned device channels";
      existing.grid = existing.grid || 4;
      existing.cameraIds = cameraIds;
      existing.system = true;
      existing.deviceId = device.id;
      existing.notes = existing.notes || "Created automatically from this device's channels.";
    } else {
      state.groups.push({
        id: groupId,
        name: `${device.name} (Assigned)`,
        purpose: "Auto-assigned device channels",
        grid: 4,
        cameraIds,
        notes: "Created automatically from this device's channels.",
        system: true,
        deviceId: device.id
      });
    }
  });

  const deviceIds = new Set(state.devices.map((device) => device.id));
  state.groups = state.groups.filter((group) => !group.system || deviceIds.has(group.deviceId));
}

function isManagedChannelPlaceholder(camera) {
  if (camera.managedPlaceholder === true) return true;
  if (camera.managedPlaceholder === false) return false;
  return (
    camera.discovered === true &&
    (camera.tags || []).includes("unmapped") &&
    camera.area === "Unassigned" &&
    camera.floor === "Unknown" &&
    camera.direction === "Direction not set"
  );
}

function removeCameraReferences(removedIds) {
  if (!removedIds.size) return;
  state.cameras = state.cameras.map((camera) => ({
    ...camera,
    related: (camera.related || []).filter((id) => !removedIds.has(id)),
    next: (camera.next || []).filter((id) => !removedIds.has(id)),
    previous: (camera.previous || []).filter((id) => !removedIds.has(id))
  }));
  removedIds.forEach((id) => {
    delete state.mapPositions[id];
  });
}

function syncDeviceChannels(device) {
  migrateCameraDeviceLinks();
  const channelCount = deviceChannelCount(device);
  device.channels = channelCount;
  const removedIds = new Set();
  let detachedCount = 0;

  state.cameras = state.cameras.flatMap((camera) => {
    if (!cameraBelongsToDevice(camera, device)) return [camera];
    const channel = Number(camera.channel);
    const managedPlaceholder = isManagedChannelPlaceholder(camera);

    if (!Number.isInteger(channel) || channel < 1 || channel > channelCount) {
      if (managedPlaceholder) {
        removedIds.add(camera.id);
        return [];
      }
      detachedCount += 1;
      return [{ ...camera, deviceId: "", nvr: "Unassigned", managedPlaceholder: false }];
    }

    return [{
      ...camera,
      deviceId: device.id,
      nvr: device.name,
      name: managedPlaceholder ? `${device.name} CH-${String(channel).padStart(2, "0")}` : camera.name,
      managedPlaceholder
    }];
  });

  const camerasByChannel = new Map();
  state.cameras
    .filter((camera) => cameraBelongsToDevice(camera, device))
    .forEach((camera) => {
      const channel = Number(camera.channel);
      const cameras = camerasByChannel.get(channel) || [];
      cameras.push(camera);
      camerasByChannel.set(channel, cameras);
    });

  camerasByChannel.forEach((cameras) => {
    if (cameras.length < 2) return;
    const configured = cameras.filter((camera) => !isManagedChannelPlaceholder(camera));
    const keep = configured[0] || cameras[0];
    cameras.forEach((camera) => {
      if (camera !== keep && isManagedChannelPlaceholder(camera)) removedIds.add(camera.id);
    });
  });

  if (removedIds.size) {
    state.cameras = state.cameras.filter((camera) => !removedIds.has(camera.id));
    removeCameraReferences(removedIds);
  }

  const existingByDeviceChannel = new Set(
    state.cameras
      .filter((camera) => cameraBelongsToDevice(camera, device))
      .map((camera) => Number(camera.channel))
  );
  let addedCount = 0;

  for (let channel = 1; channel <= channelCount; channel += 1) {
    if (existingByDeviceChannel.has(channel)) continue;
    state.cameras.push({
      id: channelCameraId(device, channel),
      name: `${device.name} CH-${String(channel).padStart(2, "0")}`,
      area: "Unassigned",
      floor: "Unknown",
      direction: "Direction not set",
      deviceId: device.id,
      nvr: device.name,
      channel,
      // A recorder state is not direct evidence of a channel/camera state.
      status: "unknown",
      tags: ["unmapped"],
      related: [],
      next: [],
      previous: [],
      discovered: true,
      managedPlaceholder: true
    });
    addedCount += 1;
  }

  return { addedCount, removedCount: removedIds.size, detachedCount };
}

function mediaStatusText(camera, mode) {
  const device = cameraDevice(camera);
  const deviceLabel = device ? `${device.host}:${device.port}` : "device not found";
  const profile = mode === "playback" ? "recording playback" : "substream preferred";
  return `${cameraDeviceName(camera)} CH-${camera.channel} | ${deviceLabel} | ${profile}`;
}

function resolveTier(context) {
  if (context.zone) {
    return resolveTierByZone(context);
  }
  if (context.isVisible === false) return "paused";
  if (context.isTracking) return "main";
  if (context.isFocused) return "main";
  if (context.paneContext === "playback" && context.tileCount >= 4) return "sub";
  if (context.tileCount >= 9) return "thumb";
  if (context.tileCount >= 2) return "sub";
  return "main";
}

function resolveTierByZone(context) {
  if (context.isTracking || context.isFocused) return "main";
  if (context.zone === "offscreen") return "paused";
  if (context.zone === "prewarm") return "thumb";

  const baseTier = context.zone === "focus" ? "main" : "thumb";
  return applyZoomCap(baseTier, context.zoomLevel || "room");
}

function applyZoomCap(baseTier, zoomLevel) {
  const rank = { paused: 0, thumb: 1, sub: 2, main: 3 };
  const cap = {
    site: "paused",
    wing: "thumb",
    room: "main"
  }[zoomLevel] || "main";

  return rank[cap] < rank[baseTier] ? cap : baseTier;
}

function tierBitrate(tier) {
  return {
    paused: 0,
    thumb: 256,
    sub: 1000,
    main: 4000
  }[tier] || 0;
}

function mediaPolicyLine(tier) {
  return state.showMediaPolicy ? `<br>Policy: ${tier} tier, approx ${tierBitrate(tier)} kbps` : "";
}

function applyTheme() {
  document.documentElement.dataset.theme = state.theme;
  if (els.statusThemeBtn) {
    const label = state.theme === "dark" ? "Switch to light theme" : "Switch to dark theme";
    els.statusThemeBtn.title = label;
    els.statusThemeBtn.setAttribute("aria-label", label);
  }
}

function cameraMatches(camera) {
  const query = state.query.trim().toLowerCase();
  const haystack = [
    camera.id,
    camera.name,
    camera.area,
    camera.floor,
    camera.direction,
    cameraDeviceName(camera),
    ...(camera.tags || [])
  ].join(" ").toLowerCase();

  return (
    (state.area === "All" || camera.area === state.area) &&
    (state.tag === "All" || (camera.tags || []).includes(state.tag)) &&
    (!query || haystack.includes(query))
  );
}

function setView(view, options = {}) {
  if (!validViews.has(view)) return;
  state.view = view;
  document.body.classList.toggle("monitor-mode", view === "operator");

  const activeNavByView = {
    catalog: "devices",
    routes: "devices",
    groups: "devices",
    compliance: "reports"
  };
  const activeNavView = activeNavByView[view] || view;
  els.navItems.forEach((item) => item.classList.toggle("active", item.dataset.view === activeNavView));
  document.querySelectorAll("[data-subnav-view]").forEach((item) => {
    item.classList.toggle("active", item.dataset.subnavView === view);
  });
  els.appViews.forEach((viewEl) => {
    const isFloatingWorkspace =
      (viewEl.id === "operatorView" && activeWorkspace("live")?.floating) ||
      (viewEl.id === "playbackView" && activeWorkspace("playback")?.floating);
    const isActive = viewEl.id === `${view}View` || isFloatingWorkspace;
    viewEl.classList.toggle("active", isActive);
  });

  persist();
  if (options.push) {
    window.history.pushState({ view }, "", `#${view}`);
  }
}

function setSelected(id) {
  if (!byId.has(id)) return;
  state.selectedId = id;
  persist();
  render();
}

function selectLiveCamera(id) {
  if (!byId.has(id)) return;
  state.selectedId = id;
  persist();
  liveSpatialCanvas?.focusCamera(id, { center: false });
  els.simpleLiveGrid?.querySelectorAll("[data-simple-live-camera]").forEach((tile) => {
    tile.classList.toggle("active", tile.dataset.simpleLiveCamera === id);
  });
  renderMonitorDrawer();
}

function selectOrOpenLiveCamera(id) {
  if (!byId.has(id)) return;
  if (!state.openIds.includes(id)) {
    if (!canAdmitCamera("live", id)) {
      persist();
      renderWorkspaceTabs("live");
      renderWorkspaceTabs("playback");
      return;
    }
    state.openIds.push(id);
  }
  state.selectedId = id;
  if (state.openIds.length > state.gridDivision) {
    state.gridDivision = Math.min(64, state.openIds.length);
  }
  persist();
  render();
}

function toggleLiveCameraMaximize(id) {
  if (!byId.has(id) || !state.openIds.includes(id)) return;
  maximizedLiveCameraId = maximizedLiveCameraId === id ? "" : id;
  state.selectedId = id;
  persist();
  renderGrid();
  renderMonitorDrawer();
}

function bindClickAndDoubleClick(element, { onClick, onDoubleClick, ignore } = {}) {
  let clickTimer = null;
  element.addEventListener("click", (event) => {
    if (ignore?.(event) || event.detail > 1) return;
    clickTimer = window.setTimeout(() => {
      clickTimer = null;
      onClick?.(event);
    }, 220);
  });
  element.addEventListener("dblclick", (event) => {
    if (ignore?.(event)) return;
    if (clickTimer) window.clearTimeout(clickTimer);
    clickTimer = null;
    event.preventDefault();
    onDoubleClick?.(event);
  });
}

function addToGrid(ids) {
  const requested = Array.isArray(ids) ? ids : [ids];
  const admitted = [];
  for (const id of requested) {
    if (byId.has(id) && !state.openIds.includes(id)) {
      if (!canAdmitCamera("live", id)) break;
      state.openIds.push(id);
      admitted.push(id);
    } else if (byId.has(id) && state.openIds.includes(id)) {
      admitted.push(id);
    }
  }
  if (state.openIds.length > state.gridDivision) {
    state.gridDivision = Math.min(64, state.openIds.length);
  }
  frameLiveViewport(admitted);
  persist();
  render();
}

function requestLiveFullscreen() {
  if (!els.livePanel || document.fullscreenElement || !els.livePanel.requestFullscreen) return;
  els.livePanel.requestFullscreen().catch(() => {});
}

function requestLiveSurfaceFullscreen() {
  const target = els.liveCanvasWrap || els.livePanel;
  if (!target || document.fullscreenElement || !target.requestFullscreen) return;
  target.requestFullscreen().catch(() => {});
}

function openSingleCamera(id, options = {}) {
  if (!byId.has(id)) return;
  const admitted = workspaceAdmissionIds("live", [id]);
  if (!admitted.length) {
    persist();
    renderWorkspaceTabs("live");
    renderWorkspaceTabs("playback");
    return;
  }
  maximizedLiveCameraId = "";
  state.selectedId = id;
  state.openIds = admitted;
  state.gridDivision = 1;
  pendingLiveFrameIds = [id];
  persist();
  render();
  if (options.fullscreen) {
    requestLiveSurfaceFullscreen();
  }
}

function frameLiveViewport(ids) {
  const cameras = ids.map((id) => byId.get(id)).filter(Boolean);
  if (!cameras.length) return;
  pendingLiveFrameIds = cameras.map((camera) => camera.id);
}

function renderChips(container, values, activeValue, onSelect) {
  container.innerHTML = "";
  values.forEach((value) => {
    const button = document.createElement("button");
    button.className = `chip ${value === activeValue ? "active" : ""}`;
    button.type = "button";
    button.textContent = value;
    button.addEventListener("click", () => onSelect(value));
    container.appendChild(button);
  });
}

function groupSectionsForCameras(cameraList) {
  const camerasInScope = new Set(cameraList.map((camera) => camera.id));
  const sections = [];
  const groupedIds = new Set();
  const sortedGroups = [...state.groups].sort((a, b) => {
    if (Boolean(a.system) !== Boolean(b.system)) return a.system ? -1 : 1;
    return a.name.localeCompare(b.name);
  });

  sortedGroups.forEach((group) => {
    const members = (group.cameraIds || [])
      .map((id) => byId.get(id))
      .filter((camera) => camera && camerasInScope.has(camera.id));
    if (!members.length) return;
    members.forEach((camera) => groupedIds.add(camera.id));
    sections.push({ group, members });
  });

  return {
    sections,
    ungrouped: cameraList.filter((camera) => !groupedIds.has(camera.id))
  };
}

function cameraRowHTML(camera) {
  const cameraTags = (camera.tags || []).slice(0, 3);
  return `
    <article class="camera-row ${camera.id === state.selectedId ? "active" : ""}" data-camera-row="${escapeHtml(camera.id)}" tabindex="0">
      <div class="camera-row-main">
        <div class="camera-row-header">
          <div class="camera-name"><span class="tree-icon">&#9673;</span>${escapeHtml(camera.name)}</div>
          <span class="status ${escapeHtml(camera.status)}">${escapeHtml(camera.status)}</span>
        </div>
        <div class="camera-meta">${escapeHtml(camera.id)} - ${escapeHtml(camera.area)} - ${escapeHtml(cameraDeviceName(camera))} CH-${escapeHtml(camera.channel)}</div>
        <div class="tag-line">${cameraTags.map((tag) => `<span class="tag">${escapeHtml(tag)}</span>`).join("")}</div>
      </div>
    </article>
  `;
}

function renderStatusBar() {
  if (!els.statusDeviceCount) return;
  const onlineDevices = state.devices.filter((device) => device.status === "online").length;
  const onlineCameras = state.cameras.filter((camera) => camera.status === "online").length;
  els.statusDeviceCount.textContent = `${onlineDevices}/${state.devices.length} devices reported online`;
  els.statusCameraCount.textContent = `${onlineCameras}/${state.cameras.length} cameras reported online`;
  renderInventoryAuthorityStatus();
}

function renderCameraList() {
  const filtered = state.cameras.filter(cameraMatches);
  els.cameraCount.textContent = `${filtered.length} shown`;
  renderCheckedCameraCount();

  if (!filtered.length) {
    els.cameraList.innerHTML = `<div class="empty-state">No matching cameras.</div>`;
    return;
  }
  const grouped = groupSectionsForCameras(filtered);
  const groupHtml = grouped.sections
    .map(({ group, members }) => {
      const isOpen = Boolean(state.query);
      return `
        <details class="camera-group-section" ${isOpen ? "open" : ""}>
          <summary>
            <span class="camera-group-title"><span class="tree-icon">&#128193;</span><span class="camera-group-name">${escapeHtml(group.name)}</span>${group.system ? ` <span class="group-badge-system">device</span>` : ""}</span>
            <span class="camera-group-actions">
              <span class="camera-group-count">${members.length}</span>
              <button class="tree-action-btn" data-open-list-group="${escapeHtml(group.id)}" type="button" title="Open all cameras in group" aria-label="Open all cameras in ${escapeHtml(group.name)}">&#9654;</button>
            </span>
          </summary>
          <div class="camera-group-members">
            ${members.map(cameraRowHTML).join("")}
          </div>
        </details>
      `;
    })
    .join("");
  const ungroupedHtml = grouped.ungrouped.length
    ? `
      <details class="camera-group-section" open>
        <summary>
          <span class="camera-group-title"><span class="camera-group-name">Ungrouped</span></span>
          <span class="camera-group-count">${grouped.ungrouped.length}</span>
        </summary>
        <div class="camera-group-members">
          ${grouped.ungrouped.map(cameraRowHTML).join("")}
        </div>
      </details>
    `
    : "";
  els.cameraList.innerHTML = groupHtml + ungroupedHtml;
  els.cameraList.querySelectorAll("[data-open-list-group]").forEach((button) => {
    button.addEventListener("click", (event) => {
      event.preventDefault();
      event.stopPropagation();
      openGroup(button.dataset.openListGroup);
    });
  });
  els.cameraList.querySelectorAll("[data-camera-row]").forEach((row) => {
    bindClickAndDoubleClick(row, {
      onClick: () => selectOrOpenLiveCamera(row.dataset.cameraRow),
      onDoubleClick: () => {
        selectOrOpenLiveCamera(row.dataset.cameraRow);
        toggleLiveCameraMaximize(row.dataset.cameraRow);
      }
    });
    row.addEventListener("keydown", (event) => {
      if (event.key === "Enter") selectOrOpenLiveCamera(row.dataset.cameraRow);
    });
  });
}

function renderCheckedCameraCount() {
  if (!els.checkedCameraCount) return;
  const selectedGroup = state.groups.find((group) => group.id === state.selectedGroupId);
  const groupName = selectedGroup ? selectedGroup.name : "no group selected";
  els.checkedCameraCount.textContent = `${checkedCameraIds.size} checked - ${groupName}`;
}

function renderLiveGroups() {
  if (!els.liveGroupList) return;
  els.liveGroupList.innerHTML = "";

  if (!state.groups.length) {
    els.liveGroupList.innerHTML = `<div class="empty-state">No groups yet.</div>`;
    renderCheckedCameraCount();
    return;
  }

  state.groups.forEach((group) => {
    const cameras = (group.cameraIds || []).filter((id) => byId.has(id));
    const row = document.createElement("article");
    row.className = `live-group-row ${group.id === state.selectedGroupId ? "active" : ""}`;
    row.innerHTML = `
      <button class="secondary" data-select-live-group="${group.id}" type="button">
        <strong>${group.name}${group.system ? ` <span class="group-badge-system">device</span>` : ""}</strong>
        <span>${cameras.length} cameras</span>
      </button>
      <button data-open-live-group="${group.id}" type="button">Open</button>
    `;
    els.liveGroupList.appendChild(row);
  });

  els.liveGroupList.querySelectorAll("[data-select-live-group]").forEach((button) => {
    button.addEventListener("click", () => {
      state.selectedGroupId = button.dataset.selectLiveGroup;
      persist();
      renderLiveGroups();
    });
  });
  els.liveGroupList.querySelectorAll("[data-open-live-group]").forEach((button) => {
    button.addEventListener("click", () => openGroup(button.dataset.openLiveGroup));
  });
  renderCheckedCameraCount();
}

function createLiveGroupFromChecked() {
  if (!requireInventoryMutation()) return;
  const name = els.quickGroupNameInput.value.trim();
  if (!name) {
    window.alert("Enter a group name.");
    return;
  }
  const cameraIds = Array.from(checkedCameraIds).filter((id) => byId.has(id));
  if (!cameraIds.length) {
    window.alert("Check at least one camera.");
    return;
  }

  const group = {
    id: `grp-${Date.now()}`,
    name,
    purpose: "Live view group",
    grid: Math.min(64, Math.max(4, cameraIds.length)),
    cameraIds,
    notes: "Created from Live View."
  };
  state.groups.unshift(group);
  state.selectedGroupId = group.id;
  state.openIds = workspaceAdmissionIds("live", cameraIds);
  state.gridDivision = group.grid;
  state.selectedId = state.openIds[0] || state.selectedId;
  checkedCameraIds.clear();
  els.quickGroupNameInput.value = "";
  frameLiveViewport(state.openIds);
  persist();
  render();
}

function addCheckedToSelectedGroup() {
  if (!requireInventoryMutation()) return;
  const group = state.groups.find((item) => item.id === state.selectedGroupId);
  if (!group) {
    window.alert("Select a group first.");
    return;
  }
  const cameraIds = Array.from(checkedCameraIds).filter((id) => byId.has(id));
  if (!cameraIds.length) {
    window.alert("Check at least one camera.");
    return;
  }
  group.cameraIds = [...new Set([...(group.cameraIds || []), ...cameraIds])];
  state.openIds = workspaceAdmissionIds("live", group.cameraIds || []);
  group.grid = Math.max(Number(group.grid || 4), Math.min(64, state.openIds.length || 1));
  state.gridDivision = group.grid;
  state.selectedId = state.openIds[0] || state.selectedId;
  checkedCameraIds.clear();
  frameLiveViewport(state.openIds);
  persist();
  render();
}

function addCamerasToGroupForm(cameraIds) {
  groupFormMemberIds = [...new Set([...groupFormMemberIds, ...cameraIds.filter((id) => byId.has(id))])];
  renderGroupCameraOptions();
  renderGroupMembersList();
}

function importSelectedCamerasIntoGroup() {
  const ids = Array.from(els.groupCameraSelect.selectedOptions).map((option) => option.value);
  if (!ids.length) {
    window.alert("Select one or more available cameras first.");
    return;
  }
  addCamerasToGroupForm(ids);
}

function importAllDeviceChannelsIntoGroup() {
  const ids = groupFormAvailableCameras().map((camera) => camera.id);
  if (!ids.length) {
    window.alert("No cameras match this device filter.");
    return;
  }
  addCamerasToGroupForm(ids);
}

function renderGrid() {
  if (liveSpatialCanvas) {
    liveSpatialCanvas.destroy();
    liveSpatialCanvas = null;
  }

  if (!state.useDraggableCanvases) {
    renderSimpleLiveGrid();
    return;
  }

  if (!els.spatialCanvasEl) return;
  els.spatialCanvasEl.style.display = "block";
  els.simpleLiveGrid.style.display = "none";
  liveSpatialCanvas = new window.SpatialCanvas(els.spatialCanvasEl, {
    onCameraSelect: (camera) => selectLiveCamera(camera.id),
    onCameraDoubleClick: (camera) => toggleLiveCameraMaximize(camera.id),
    onTierChange: () => {},
    maxActiveStreams: effectiveResourceLimit(),
    division: maximizedLiveCameraId ? 1 : state.gridDivision
  });

  const openCameras = state.openIds.map((id) => byId.get(id)).filter(Boolean);
  const maximizedCamera = byId.get(maximizedLiveCameraId);
  if (maximizedLiveCameraId && !maximizedCamera) maximizedLiveCameraId = "";
  const camerasToShow = maximizedCamera ? [maximizedCamera] : openCameras;
  if (!camerasToShow.length) {
    liveSpatialCanvas.setCameras([]);
    return;
  }
  const laidOut = window.SpatialCanvasLayout.layoutGrid(camerasToShow, maximizedCamera ? 1 : state.gridDivision);
  liveSpatialCanvas.setCameras(laidOut);
  if (state.selectedId) {
    liveSpatialCanvas.focusCamera(state.selectedId, { center: false });
  }
  if (pendingLiveFrameIds.length) {
    liveSpatialCanvas.frameCameras(pendingLiveFrameIds);
    pendingLiveFrameIds = [];
  }
}

function applyGridLayout(gridEl, slotCount) {
  const { columns, rows } = gridMatrix(slotCount);
  gridEl.style.gridTemplateColumns = `repeat(${columns}, minmax(0, 1fr))`;
  gridEl.style.gridTemplateRows = `repeat(${rows}, minmax(0, 1fr))`;
  gridEl.style.gridAutoRows = "minmax(0, 1fr)";
}

function gridMatrix(slotCount) {
  const count = Math.max(1, Math.round(Number(slotCount) || 1));
  const columns = Math.max(1, Math.ceil(Math.sqrt(count)));
  const rows = Math.max(1, Math.ceil(count / columns));
  return { columns, rows };
}

function renderSimpleLiveGrid() {
  if (els.spatialCanvasEl) {
    els.spatialCanvasEl.style.display = "none";
  }

  const openCameras = state.openIds.map((id) => byId.get(id)).filter(Boolean);
  const maximizedCamera = byId.get(maximizedLiveCameraId);
  if (maximizedLiveCameraId && !maximizedCamera) maximizedLiveCameraId = "";
  const slotCount = maximizedCamera ? 1 : Math.max(1, state.gridDivision);
  const camerasToShow = maximizedCamera ? [maximizedCamera] : openCameras.slice(0, slotCount);

  els.simpleLiveGrid.style.display = "grid";
  els.simpleLiveGrid.className = `simple-live-grid ${slotCount === 1 ? "focused-feed-grid" : ""}`;
  applyGridLayout(els.simpleLiveGrid, slotCount);

  if (!camerasToShow.length) {
  }

  const tileHtml = camerasToShow.map((camera) => {
    const tier = resolveTier({
      tileCount: camerasToShow.length,
      isFocused: camera.id === state.selectedId || camerasToShow.length === 1,
      isTracking: false,
      paneContext: "grid",
      isVisible: true
    });
    return `
      <article class="simple-video-tile ${camera.id === state.selectedId ? "active" : ""}" data-simple-live-camera="${camera.id}">
        <div class="tile-header">
          <strong>${camera.name}</strong>
          <span class="status ${camera.status}">${tier}</span>
        </div>
        <div class="simple-video-body">
          <div class="digital-zoom-stage" style="transform: scale(${camera.id === state.selectedId ? state.liveDigitalZoom : 1})">
            <strong>Live bridge pending</strong>
            ${mediaStatusText(camera, "live")}${mediaPolicyLine(tier)}
          </div>
        </div>
        <div class="tile-footer">
          <span>${camera.id}</span>
          <span>${camera.area}</span>
        </div>
      </article>
    `;
  });
  const emptySlotHtml = `<div class="simple-video-tile empty-slot">Empty channel</div>`;
  const emptyCount = Math.max(0, slotCount - camerasToShow.length);
  els.simpleLiveGrid.innerHTML = tileHtml.join("") + emptySlotHtml.repeat(emptyCount);

  els.simpleLiveGrid.querySelectorAll("[data-simple-live-camera]").forEach((tile) => {
    bindClickAndDoubleClick(tile, {
      onClick: () => selectLiveCamera(tile.dataset.simpleLiveCamera),
      onDoubleClick: () => toggleLiveCameraMaximize(tile.dataset.simpleLiveCamera)
    });
  });
}

function setLiveDigitalZoom(value) {
  const clamped = clampNumber(value, 1, 8, 1);
  state.liveDigitalZoom = Math.round(clamped * 4) / 4;
  persist();
  renderGrid();
  renderMonitorDrawer();
}

function queuePtzAction(action) {
  const camera = byId.get(state.selectedId);
  if (!camera) {
    return {
      status: "unavailable",
      reasonCode: "CAMERA_NOT_SELECTED",
      message: "Select a camera before checking PTZ availability."
    };
  }
  const label = action.replace(/-/g, " ");
  const message = `${camera.name}: ${label} unavailable - no verified PTZ adapter operation is implemented.`;
  if (els.ptzStatus) els.ptzStatus.textContent = message;
  if (els.monitorDrawerStatus) els.monitorDrawerStatus.textContent = "PTZ unavailable";
  if (els.statusMessage) els.statusMessage.textContent = message;
  return {
    status: "unavailable",
    reasonCode: "OPERATION_NOT_IMPLEMENTED",
    message
  };
}

function renderMonitorDrawer() {
  if (!els.monitorDrawerBody) return;
  const camera = byId.get(state.selectedId);
  if (!camera) {
    els.monitorDrawerTitle.textContent = "No camera selected";
    els.monitorDrawerStatus.textContent = "idle";
    els.monitorDrawerBody.innerHTML = `<div class="empty-state">Select a camera from the tree or live stage.</div>`;
    return;
  }

  const device = cameraDevice(camera);
  const related = (camera.related || [])
    .map((id) => byId.get(id))
    .filter(Boolean)
    .slice(0, 6);
  const tagsHtml = (camera.tags || []).map((tag) => `<span>${escapeHtml(tag)}</span>`).join("") || "<span>untagged</span>";
  const relatedHtml = related.length
    ? related.map((item) => `<button data-drawer-open-camera="${escapeHtml(item.id)}" type="button">${escapeHtml(item.name)}</button>`).join("")
    : `<span class="detail-muted">No related cameras mapped yet.</span>`;

  els.monitorDrawerTitle.textContent = camera.name;
  els.monitorDrawerStatus.textContent = camera.status || "unknown";
  els.monitorDrawerStatus.className = `status ${camera.status || "idle"}`;
  els.monitorDrawerBody.innerHTML = `
    <section class="drawer-section">
      <div class="detail-row"><span>ID</span><strong>${escapeHtml(camera.id)}</strong></div>
      <div class="detail-row"><span>Location</span><strong>${escapeHtml([camera.area, camera.floor].filter(Boolean).join(" / ") || "Unassigned")}</strong></div>
      <div class="detail-row"><span>Device</span><strong>${escapeHtml(device?.name || camera.nvr || "Standalone")}</strong></div>
      <div class="detail-row"><span>Channel</span><strong>${escapeHtml(camera.channel || "IP")}</strong></div>
      <div class="detail-row"><span>Direction</span><strong>${escapeHtml(camera.direction || "Not set")}</strong></div>
      <div class="tag-strip">${tagsHtml}</div>
    </section>

    <section class="drawer-section">
      <div class="drawer-section-title">Digital Zoom</div>
      <div class="zoom-control-row">
        <button data-live-zoom="-0.25" type="button">-</button>
        <strong>${Math.round(state.liveDigitalZoom * 100)}%</strong>
        <button data-live-zoom="0.25" type="button">+</button>
      </div>
      <input data-live-zoom-range type="range" min="1" max="8" step="0.25" value="${state.liveDigitalZoom}" />
    </section>

    <section class="drawer-section">
      <div class="drawer-section-title">PTZ</div>
      <div class="ptz-pad">
        <button data-ptz-action="up-left" type="button">&#8598;</button>
        <button data-ptz-action="up" type="button">&#8593;</button>
        <button data-ptz-action="up-right" type="button">&#8599;</button>
        <button data-ptz-action="left" type="button">&#8592;</button>
        <button data-ptz-action="stop" type="button">Stop</button>
        <button data-ptz-action="right" type="button">&#8594;</button>
        <button data-ptz-action="down-left" type="button">&#8601;</button>
        <button data-ptz-action="down" type="button">&#8595;</button>
        <button data-ptz-action="down-right" type="button">&#8600;</button>
      </div>
      <div class="drawer-actions">
        <button data-ptz-action="zoom-in" type="button">PTZ Zoom +</button>
        <button data-ptz-action="zoom-out" type="button">PTZ Zoom -</button>
      </div>
    </section>

    <section class="drawer-section">
      <div class="drawer-section-title">Nearby</div>
      <div class="drawer-actions">${relatedHtml}</div>
    </section>
  `;

  els.monitorDrawerBody.querySelectorAll("[data-drawer-open-camera]").forEach((button) => {
    button.addEventListener("click", () => openSingleCamera(button.dataset.drawerOpenCamera));
  });
  els.monitorDrawerBody.querySelectorAll("[data-live-zoom]").forEach((button) => {
    button.addEventListener("click", () => setLiveDigitalZoom(state.liveDigitalZoom + Number(button.dataset.liveZoom)));
  });
  els.monitorDrawerBody.querySelector("[data-live-zoom-range]")?.addEventListener("input", (event) => {
    setLiveDigitalZoom(event.target.value);
  });
  els.monitorDrawerBody.querySelectorAll("[data-ptz-action]").forEach((button) => {
    button.addEventListener("click", () => queuePtzAction(button.dataset.ptzAction));
  });
}

const checkedDeviceIds = new Set(); // ephemeral, table bulk-select only

function renderDevices() {
  const query = (els.deviceFilterInput?.value || "").trim().toLowerCase();
  const filtered = state.devices.filter((device) => {
    if (!query) return true;
    return [device.name, device.type, device.vendor, device.host].join(" ").toLowerCase().includes(query);
  });

  els.deviceCount.textContent = `Total (${state.devices.length})`;
  els.deviceTableBody.innerHTML = "";

  if (!filtered.length) {
    els.deviceTableBody.innerHTML = `<tr><td colspan="9" class="empty-state">No devices match.</td></tr>`;
    return;
  }

  filtered.forEach((device) => {
    const cameraCount = state.cameras.filter((camera) => cameraBelongsToDevice(camera, device)).length;
    const row = document.createElement("tr");
    row.innerHTML = `
      <td class="col-check"><input type="checkbox" data-device-checkbox="${device.id}" ${checkedDeviceIds.has(device.id) ? "checked" : ""} /></td>
      <td>${device.name}</td>
      <td>IP/Domain</td>
      <td>${device.host}:${device.port}</td>
      <td>${device.type}${device.vendor ? ` · ${device.vendor}` : ""}</td>
      <td>${device.channels}</td>
      <td>${cameraCount}</td>
      <td><span class="status ${device.status}">● ${device.status}</span></td>
      <td class="col-ops">
        <button class="icon-btn" data-edit-device="${device.id}" title="Edit" type="button">✎</button>
        <button class="icon-btn" data-sync-device="${device.id}" title="Sync Channels" type="button">⟳</button>
        <button class="icon-btn danger" data-delete-device="${device.id}" title="Delete" type="button">🗑</button>
      </td>
    `;
    els.deviceTableBody.appendChild(row);
  });

  els.deviceTableBody.querySelectorAll("[data-edit-device]").forEach((button) => {
    button.addEventListener("click", () => loadDeviceIntoForm(button.dataset.editDevice));
  });
  els.deviceTableBody.querySelectorAll("[data-sync-device]").forEach((button) => {
    button.addEventListener("click", () => {
      if (!requireInventoryMutation()) return;
      const device = state.devices.find((item) => item.id === button.dataset.syncDevice);
      if (!device) return;
      syncDeviceChannels(device);
      refreshCameraIndexes();
      persist();
      render();
    });
  });
  els.deviceTableBody.querySelectorAll("[data-delete-device]").forEach((button) => {
    button.addEventListener("click", () => deleteDevice(button.dataset.deleteDevice));
  });
  els.deviceTableBody.querySelectorAll("[data-device-checkbox]").forEach((box) => {
    box.addEventListener("change", () => {
      if (box.checked) checkedDeviceIds.add(box.dataset.deviceCheckbox);
      else checkedDeviceIds.delete(box.dataset.deviceCheckbox);
    });
  });
  if (els.deviceSelectAll) {
    els.deviceSelectAll.checked = filtered.length > 0 && filtered.every((d) => checkedDeviceIds.has(d.id));
  }
}

function applyDeviceTypeChannelRule({ restoreRecorderDefault = false } = {}) {
  const directIpCamera = isIpCameraDirectType(els.deviceType.value);
  const wasDirectIpCamera = els.deviceChannels.dataset.directIpCamera === "true";

  if (directIpCamera) {
    els.deviceChannels.value = 1;
    els.deviceChannels.disabled = true;
    els.deviceChannels.dataset.directIpCamera = "true";
    els.deviceChannels.title = "Direct IP cameras use one channel.";
    return;
  }

  els.deviceChannels.disabled = false;
  els.deviceChannels.dataset.directIpCamera = "false";
  els.deviceChannels.removeAttribute("title");
  if (restoreRecorderDefault && wasDirectIpCamera) els.deviceChannels.value = 16;
}

function resetDeviceForm() {
  els.deviceForm.reset();
  els.deviceId.value = "";
  els.devicePort.value = "";
  els.deviceChannels.value = 16;
  els.deviceStatus.value = "unknown";
  applyDeviceTypeChannelRule();
}

function loadDeviceIntoForm(id) {
  const device = state.devices.find((item) => item.id === id);
  if (!device) return;
  els.deviceId.value = device.id;
  els.deviceName.value = device.name;
  els.deviceType.value = device.type;
  els.deviceVendor.value = device.vendor || "";
  els.deviceHost.value = device.host;
  els.devicePort.value = device.port;
  els.deviceChannels.value = deviceChannelCount(device);
  els.deviceStatus.value = device.status;
  els.deviceUsername.value = device.username || "";
  els.devicePassword.value = device.password || "";
  els.deviceNotes.value = device.notes || "";
  applyDeviceTypeChannelRule();
}

function saveDevice(event) {
  event.preventDefault();
  if (!requireInventoryMutation()) return;

  const type = els.deviceType.value;
  const port = Number(els.devicePort.value);
  if (!Number.isInteger(port) || port < 1 || port > 65535) {
    window.alert("Enter the device-specific port from 1 to 65535.");
    return;
  }
  const device = {
    id: els.deviceId.value || `dev-${Date.now()}`,
    name: els.deviceName.value.trim(),
    type,
    vendor: els.deviceVendor.value.trim(),
    host: els.deviceHost.value.trim(),
    port,
    channels: isIpCameraDirectType(type)
      ? 1
      : Math.round(clampNumber(els.deviceChannels.value, 1, 256, 1)),
    status: els.deviceStatus.value,
    username: els.deviceUsername.value.trim(),
    password: els.devicePassword.value,
    notes: els.deviceNotes.value.trim()
  };

  const existingIndex = state.devices.findIndex((item) => item.id === device.id);
  if (existingIndex >= 0) {
    state.devices[existingIndex] = device;
  } else {
    state.devices.push(device);
  }
  syncDeviceChannels(device);
  refreshCameraIndexes();

  persist();
  resetDeviceForm();
  render();
}

function detachDeviceCameras(deviceIds) {
  migrateCameraDeviceLinks();
  const removedIds = new Set();
  state.cameras = state.cameras.flatMap((camera) => {
    if (!deviceIds.has(camera.deviceId)) return [camera];
    if (isManagedChannelPlaceholder(camera)) {
      removedIds.add(camera.id);
      return [];
    }
    return [{ ...camera, deviceId: "", nvr: "Unassigned", managedPlaceholder: false }];
  });
  removeCameraReferences(removedIds);
}

function deleteDevice(id) {
  if (!requireInventoryMutation()) return;
  const device = state.devices.find((item) => item.id === id);
  const hasMappedCameras = device && state.cameras.some((camera) => cameraBelongsToDevice(camera, device));
  const message = hasMappedCameras
    ? "Delete this device? Generated channel placeholders will be removed; configured cameras will be kept as unassigned."
    : "Delete this device?";

  if (!window.confirm(message)) return;
  detachDeviceCameras(new Set([id]));
  state.devices = state.devices.filter((item) => item.id !== id);
  refreshCameraIndexes();
  persist();
  resetDeviceForm();
  render();
}

function renderCameraDeviceOptions(selectedDeviceId = "") {
  els.cameraDeviceInput.innerHTML = "";
  const unassignedOption = document.createElement("option");
  unassignedOption.value = "";
  unassignedOption.textContent = "Unassigned / standalone";
  unassignedOption.selected = !selectedDeviceId;
  els.cameraDeviceInput.appendChild(unassignedOption);

  state.devices.forEach((device) => {
    const option = document.createElement("option");
    option.value = device.id;
    option.textContent = `${device.name} (${device.host})`;
    option.selected = device.id === selectedDeviceId;
    els.cameraDeviceInput.appendChild(option);
  });
}

function renderCameraChannelOptions(selectedDeviceId = "", selectedChannel = 1) {
  const device = state.devices.find((item) => item.id === selectedDeviceId);
  const channelCount = device ? deviceChannelCount(device) : Math.max(1, Number(selectedChannel || 1));
  els.cameraChannelSelect.innerHTML = "";

  for (let channel = 1; channel <= channelCount; channel += 1) {
    const existing = device
      ? state.cameras.find((camera) => cameraBelongsToDevice(camera, device) && Number(camera.channel) === channel)
      : undefined;
    const option = document.createElement("option");
    option.value = String(channel);
    option.textContent = existing ? `CH-${channel}: ${existing.name}` : `CH-${channel}: Unmapped`;
    option.selected = Number(selectedChannel) === channel;
    els.cameraChannelSelect.appendChild(option);
  }
  els.cameraChannelInput.value = selectedChannel;
}

function resetCameraForm() {
  els.cameraForm.reset();
  els.cameraEditId.value = "";
  els.cameraChannelInput.value = 1;
  els.cameraStatusInput.value = "unknown";
  renderCameraDeviceOptions(state.devices[0]?.id || "");
  renderCameraChannelOptions(els.cameraDeviceInput.value, 1);
}

function loadCameraIntoForm(id) {
  const camera = byId.get(id);
  if (!camera) return;

  els.cameraEditId.value = camera.id;
  els.cameraIdInput.value = camera.id;
  els.cameraChannelInput.value = camera.channel;
  els.cameraNameInput.value = camera.name;
  const deviceId = cameraDevice(camera)?.id || "";
  renderCameraDeviceOptions(deviceId);
  renderCameraChannelOptions(deviceId, camera.channel);
  els.cameraAreaInput.value = camera.area;
  els.cameraFloorInput.value = camera.floor;
  els.cameraDirectionInput.value = camera.direction;
  els.cameraTagsInput.value = (camera.tags || []).join(", ");
  els.cameraRelatedInput.value = (camera.related || []).join(", ");
  els.cameraPreviousInput.value = (camera.previous || []).join(", ");
  els.cameraNextInput.value = (camera.next || []).join(", ");
  els.cameraStatusInput.value = camera.status;
}

function saveCamera(event) {
  event.preventDefault();
  if (!requireInventoryMutation()) return;

  const editingId = els.cameraEditId.value;
  const id = els.cameraIdInput.value.trim();
  const selectedDeviceId = els.cameraDeviceInput.value;
  const selectedDevice = state.devices.find((device) => device.id === selectedDeviceId);
  const selectedChannel = Number(els.cameraChannelSelect.value || els.cameraChannelInput.value || 1);
  const duplicate = state.cameras.some((camera) => camera.id === id && camera.id !== editingId);

  if (duplicate) {
    window.alert("A camera with this ID already exists.");
    return;
  }

  const channelOwner = selectedDevice
    ? state.cameras.find(
      (camera) => cameraBelongsToDevice(camera, selectedDevice) && Number(camera.channel) === selectedChannel && camera.id !== editingId
    )
    : undefined;

  if (channelOwner && !isManagedChannelPlaceholder(channelOwner)) {
    window.alert(`Channel ${selectedChannel} is already assigned to ${channelOwner.name}.`);
    return;
  }

  const camera = {
    id,
    name: els.cameraNameInput.value.trim(),
    area: els.cameraAreaInput.value.trim() || "Unassigned",
    floor: els.cameraFloorInput.value.trim() || "Unknown",
    direction: els.cameraDirectionInput.value.trim() || "Direction not set",
    deviceId: selectedDevice?.id || "",
    nvr: selectedDevice?.name || "Unassigned",
    channel: selectedChannel,
    status: els.cameraStatusInput.value,
    tags: csvToList(els.cameraTagsInput.value),
    related: csvToList(els.cameraRelatedInput.value),
    previous: csvToList(els.cameraPreviousInput.value),
    next: csvToList(els.cameraNextInput.value),
    managedPlaceholder: false
  };

  const existingIndex = state.cameras.findIndex((item) => item.id === editingId);
  const channelOwnerIndex = channelOwner ? state.cameras.findIndex((item) => item.id === channelOwner.id) : -1;
  if (existingIndex >= 0) {
    state.cameras[existingIndex] = camera;
    if (channelOwnerIndex >= 0) {
      const removedIds = new Set([channelOwner.id]);
      state.cameras = state.cameras.filter((item) => !removedIds.has(item.id));
      removeCameraReferences(removedIds);
    }
    state.openIds = state.openIds.map((openId) => (openId === editingId ? camera.id : openId));
    if (state.selectedId === editingId) {
      state.selectedId = camera.id;
    }
  } else if (channelOwnerIndex >= 0) {
    state.cameras[channelOwnerIndex] = {
      ...camera,
      id: channelOwner.id,
      discovered: channelOwner.discovered,
      managedPlaceholder: false
    };
    state.selectedId = channelOwner.id;
  } else {
    state.cameras.push(camera);
    state.selectedId = camera.id;
  }

  refreshCameraIndexes();
  persist();
  resetCameraForm();
  render();
}

function deleteCamera(id) {
  if (!requireInventoryMutation()) return;
  const camera = byId.get(id);
  if (!camera) return;
  if (!window.confirm(`Delete camera ${camera.name}?`)) return;

  state.cameras = state.cameras
    .filter((item) => item.id !== id)
    .map((item) => ({
      ...item,
      related: (item.related || []).filter((relatedId) => relatedId !== id),
      previous: (item.previous || []).filter((previousId) => previousId !== id),
      next: (item.next || []).filter((nextId) => nextId !== id)
    }));
  state.openIds = state.openIds.filter((openId) => openId !== id);
  delete state.mapPositions[id];

  refreshCameraIndexes();
  persist();
  resetCameraForm();
  render();
}

function uniqueList(items) {
  return [...new Set(items.filter(Boolean))];
}

function updateCameraById(id, updater) {
  const index = state.cameras.findIndex((camera) => camera.id === id);
  if (index < 0) return;
  state.cameras[index] = updater({ ...state.cameras[index] });
}

function setRouteSelected(id) {
  if (!byId.has(id)) return;
  state.routeSelectedId = id;
  persist();
  renderRoutes();
}

function addRouteLink(targetId, type) {
  if (!requireInventoryMutation()) return;
  const sourceId = state.routeSelectedId;
  if (!sourceId || !targetId || sourceId === targetId) return;

  if (type === "next") {
    updateCameraById(sourceId, (camera) => ({ ...camera, next: uniqueList([...(camera.next || []), targetId]) }));
    updateCameraById(targetId, (camera) => ({ ...camera, previous: uniqueList([...(camera.previous || []), sourceId]) }));
  }

  if (type === "previous") {
    updateCameraById(sourceId, (camera) => ({ ...camera, previous: uniqueList([...(camera.previous || []), targetId]) }));
    updateCameraById(targetId, (camera) => ({ ...camera, next: uniqueList([...(camera.next || []), sourceId]) }));
  }

  if (type === "related") {
    updateCameraById(sourceId, (camera) => ({ ...camera, related: uniqueList([...(camera.related || []), targetId]) }));
    updateCameraById(targetId, (camera) => ({ ...camera, related: uniqueList([...(camera.related || []), sourceId]) }));
  }

  refreshCameraIndexes();
  persist();
  render();
}

function removeRouteLink(targetId, type) {
  if (!requireInventoryMutation()) return;
  const sourceId = state.routeSelectedId;
  if (!sourceId || !targetId) return;

  if (type === "next") {
    updateCameraById(sourceId, (camera) => ({ ...camera, next: (camera.next || []).filter((id) => id !== targetId) }));
    updateCameraById(targetId, (camera) => ({ ...camera, previous: (camera.previous || []).filter((id) => id !== sourceId) }));
  }

  if (type === "previous") {
    updateCameraById(sourceId, (camera) => ({ ...camera, previous: (camera.previous || []).filter((id) => id !== targetId) }));
    updateCameraById(targetId, (camera) => ({ ...camera, next: (camera.next || []).filter((id) => id !== sourceId) }));
  }

  if (type === "related") {
    updateCameraById(sourceId, (camera) => ({ ...camera, related: (camera.related || []).filter((id) => id !== targetId) }));
    updateCameraById(targetId, (camera) => ({ ...camera, related: (camera.related || []).filter((id) => id !== sourceId) }));
  }

  refreshCameraIndexes();
  persist();
  render();
}

function renderCatalog() {
  els.catalogCount.textContent = `${state.cameras.length} cameras`;
  els.catalogTable.innerHTML = "";
  renderCameraDeviceOptions(els.cameraDeviceInput.value);

  if (!state.cameras.length) {
    els.catalogTable.innerHTML = `<div class="empty-state">No cameras added yet.</div>`;
    return;
  }

  state.cameras.forEach((camera) => {
    const row = document.createElement("article");
    row.className = "table-row";
    row.innerHTML = `
      <div>
        <div class="camera-name">${camera.name}</div>
        <div class="camera-meta">${camera.id} - ${camera.area} - ${camera.floor} - ${camera.direction}</div>
        <div class="tag-line">${(camera.tags || []).map((tag) => `<span class="tag">${tag}</span>`).join("")}</div>
      </div>
      <div class="metric"><span>Device</span><strong>${cameraDeviceName(camera)} CH-${camera.channel}</strong></div>
      <div class="device-actions">
        <button data-live-camera="${camera.id}" type="button">Live</button>
        <button class="secondary" data-playback-camera="${camera.id}" type="button">Playback</button>
        <button class="secondary" data-edit-camera="${camera.id}" type="button">Edit</button>
        <button class="danger" data-delete-camera="${camera.id}" type="button">Delete</button>
      </div>
    `;
    els.catalogTable.appendChild(row);
  });

  document.querySelectorAll("[data-edit-camera]").forEach((button) => {
    button.addEventListener("click", () => loadCameraIntoForm(button.dataset.editCamera));
  });
  document.querySelectorAll("[data-live-camera]").forEach((button) => {
    button.addEventListener("click", () => {
      setSelected(button.dataset.liveCamera);
      addToGrid(button.dataset.liveCamera);
      setView("operator", { push: true });
    });
  });
  document.querySelectorAll("[data-playback-camera]").forEach((button) => {
    button.addEventListener("click", () => {
      addToPlaybackGrid(button.dataset.playbackCamera);
      setView("playback", { push: true });
    });
  });
  document.querySelectorAll("[data-delete-camera]").forEach((button) => {
    button.addEventListener("click", () => deleteCamera(button.dataset.deleteCamera));
  });
}

function renderRoutes() {
  els.routeCount.textContent = `${state.cameras.length} cameras`;
  els.routeFocus.innerHTML = "";
  els.routeCandidates.innerHTML = "";
  els.routeList.innerHTML = "";

  if (!state.cameras.length) {
    els.routeFocus.innerHTML = `<div class="empty-state">Add cameras before building routes.</div>`;
    els.routeList.innerHTML = `<div class="empty-state">Add cameras before building routes.</div>`;
    return;
  }

  const selected = byId.get(state.routeSelectedId) || state.cameras[0];
  state.routeSelectedId = selected.id;

  els.routeFocus.innerHTML = `
    <article class="route-summary">
      <div class="camera-name">${selected.name}</div>
      <div class="camera-meta">${selected.id} - ${selected.area} - ${selected.direction}</div>
      <div class="detail-row"><strong>Device</strong><span>${cameraDeviceName(selected)} CH-${selected.channel}</span></div>
      <div class="detail-row"><strong>Tags</strong><span>${(selected.tags || []).join(", ") || "No tags"}</span></div>
      <div class="form-actions">
        <button id="routeOpenSelectedBtn" type="button">Open</button>
        <button id="routeEditSelectedBtn" class="secondary" type="button">Edit</button>
      </div>
    </article>
    ${renderRouteLinkGroup("Previous", selected.previous || [], "previous")}
    ${renderRouteLinkGroup("Next", selected.next || [], "next")}
    ${renderRouteLinkGroup("Nearby", selected.related || [], "related")}
  `;

  document.getElementById("routeOpenSelectedBtn").addEventListener("click", () => {
    setSelected(selected.id);
    addToGrid(selected.id);
    setView("operator", { push: true });
  });
  document.getElementById("routeEditSelectedBtn").addEventListener("click", () => {
    loadCameraIntoForm(selected.id);
    setView("catalog", { push: true });
  });

  document.querySelectorAll("[data-remove-route]").forEach((button) => {
    button.addEventListener("click", () => removeRouteLink(button.dataset.removeRoute, button.dataset.routeType));
  });
  document.querySelectorAll("[data-focus-route]").forEach((button) => {
    button.addEventListener("click", () => setRouteSelected(button.dataset.focusRoute));
  });

  els.routeCandidates.innerHTML = `
    <div class="candidate-toolbar">
      <input id="routeSearchInput" placeholder="Find camera to connect" value="${state.routeQuery}" />
      <select id="routeAreaInput">
        ${areas.map((area) => `<option value="${area}" ${area === state.routeArea ? "selected" : ""}>${area}</option>`).join("")}
      </select>
    </div>
  `;

  const candidates = state.cameras.filter((camera) => {
    const query = state.routeQuery.trim().toLowerCase();
    const haystack = [camera.id, camera.name, camera.area, camera.direction, ...(camera.tags || [])].join(" ").toLowerCase();
    return camera.id !== selected.id && (state.routeArea === "All" || camera.area === state.routeArea) && (!query || haystack.includes(query));
  });

  const grid = document.createElement("div");
  grid.className = "candidate-grid";
  candidates.forEach((camera) => {
    const card = document.createElement("article");
    card.className = "candidate-card";
    card.innerHTML = `
      <div class="camera-row-header">
        <div>
          <div class="camera-name">${camera.name}</div>
          <div class="camera-meta">${camera.id} - ${camera.area}</div>
        </div>
        <span class="status ${camera.status}">${camera.status}</span>
      </div>
      <div class="tag-line">${(camera.tags || []).map((tag) => `<span class="tag">${tag}</span>`).join("")}</div>
      <div class="candidate-actions">
        <button class="secondary" data-add-route="${camera.id}" data-route-type="previous" type="button">Prev</button>
        <button data-add-route="${camera.id}" data-route-type="next" type="button">Next</button>
        <button class="secondary" data-add-route="${camera.id}" data-route-type="related" type="button">Near</button>
      </div>
    `;
    grid.appendChild(card);
  });

  if (!candidates.length) {
    grid.innerHTML = `<div class="empty-state">No cameras match this route filter.</div>`;
  }

  els.routeCandidates.appendChild(grid);

  document.getElementById("routeSearchInput").addEventListener("input", (event) => {
    state.routeQuery = event.target.value;
    renderRoutes();
  });
  document.getElementById("routeAreaInput").addEventListener("change", (event) => {
    state.routeArea = event.target.value;
    renderRoutes();
  });
  document.querySelectorAll("[data-add-route]").forEach((button) => {
    button.addEventListener("click", () => addRouteLink(button.dataset.addRoute, button.dataset.routeType));
  });

  state.cameras.forEach((camera) => {
    const card = document.createElement("article");
    card.className = `route-card ${camera.id === selected.id ? "active" : ""}`;
    card.innerHTML = `
      <div class="route-card-header">
        <div>
          <div class="camera-name">${camera.name}</div>
          <div class="camera-meta">${camera.area} - ${camera.direction}</div>
        </div>
        <button class="secondary" data-select-route-camera="${camera.id}" type="button">Select</button>
      </div>
      <div class="detail-row"><strong>Previous</strong><span>${(camera.previous || []).map((id) => byId.get(id)?.name || id).join(", ") || "Not mapped"}</span></div>
      <div class="detail-row"><strong>Next</strong><span>${(camera.next || []).map((id) => byId.get(id)?.name || id).join(", ") || "Not mapped"}</span></div>
    `;
    els.routeList.appendChild(card);
  });

  document.querySelectorAll("[data-select-route-camera]").forEach((button) => {
    button.addEventListener("click", () => setRouteSelected(button.dataset.selectRouteCamera));
  });
}

function renderRouteLinkGroup(title, ids, type) {
  const links = ids
    .map((id) => byId.get(id))
    .filter(Boolean)
    .map((camera) => `
      <div class="route-link">
        <button class="secondary" data-focus-route="${camera.id}" type="button">${camera.name}</button>
        <button class="danger" data-remove-route="${camera.id}" data-route-type="${type}" type="button">Remove</button>
      </div>
    `)
    .join("");

  return `
    <article class="route-summary">
      <div class="panel-title">${title}</div>
      <div class="route-link-list">
        ${links || `<div class="empty-state">No ${title.toLowerCase()} cameras mapped.</div>`}
      </div>
    </article>
  `;
}

function defaultMapPosition(camera, index, total) {
  const areaSeed = [...camera.area].reduce((sum, char) => sum + char.charCodeAt(0), 0);
  const angle = ((index + 1) / Math.max(total, 1)) * Math.PI * 2;
  const radius = 24 + (areaSeed % 16);
  return {
    x: Math.round(50 + Math.cos(angle) * radius),
    y: Math.round(50 + Math.sin(angle) * radius),
    facing: (areaSeed + index * 45) % 360
  };
}

function getMapPosition(camera, index, total) {
  return state.mapPositions[camera.id] || defaultMapPosition(camera, index, total);
}

function setMapSelected(id) {
  if (!byId.has(id)) return;
  state.mapSelectedId = id;
  const camera = byId.get(id);
  if (camera && state.mapArea !== "All" && camera.area !== state.mapArea) {
    state.mapArea = camera.area;
  }
  persist();
  renderMap();
}

function renderMapOptions(visibleCameras) {
  els.mapAreaInput.innerHTML = areas.map((area) => `<option value="${area}" ${area === state.mapArea ? "selected" : ""}>${area}</option>`).join("");
  els.mapCameraInput.innerHTML = state.cameras
    .map((camera) => `<option value="${camera.id}" ${camera.id === state.mapSelectedId ? "selected" : ""}>${camera.name}</option>`)
    .join("");

  const selected = byId.get(state.mapSelectedId) || visibleCameras[0] || state.cameras[0];
  if (!selected) return;
  const position = getMapPosition(selected, state.cameras.findIndex((camera) => camera.id === selected.id), state.cameras.length);
  els.mapCameraInput.value = selected.id;
  els.mapXInput.value = position.x;
  els.mapYInput.value = position.y;
  els.mapFacingInput.value = String(position.facing);
}

function renderMap() {
  els.siteMap.innerHTML = "";
  els.mapCameraDetail.innerHTML = "";

  if (!state.cameras.length) {
    els.mapCount.textContent = "No cameras";
    els.siteMap.innerHTML = `<div class="empty-state">Add cameras before placing them on a map.</div>`;
    return;
  }

  const visibleCameras = state.cameras.filter((camera) => state.mapArea === "All" || camera.area === state.mapArea);
  if (!visibleCameras.some((camera) => camera.id === state.mapSelectedId)) {
    state.mapSelectedId = visibleCameras[0]?.id || state.cameras[0].id;
  }

  renderMapOptions(visibleCameras);
  els.mapCount.textContent = `${visibleCameras.length} shown`;

  const zone = document.createElement("div");
  zone.className = "map-zone-label";
  zone.textContent = state.mapArea === "All" ? "All areas" : state.mapArea;
  els.siteMap.appendChild(zone);

  visibleCameras.forEach((camera, index) => {
    const position = getMapPosition(camera, index, visibleCameras.length);
    const marker = document.createElement("button");
    marker.className = `map-marker ${camera.status} ${camera.id === state.mapSelectedId ? "active" : ""}`;
    marker.type = "button";
    marker.style.left = `${position.x}%`;
    marker.style.top = `${position.y}%`;
    marker.style.setProperty("--facing", `${position.facing}deg`);
    marker.innerHTML = `
      ${camera.id.split("-").pop()}
      <span class="map-marker-label">${camera.name}</span>
    `;
    marker.addEventListener("click", () => setMapSelected(camera.id));
    els.siteMap.appendChild(marker);
  });

  const selected = byId.get(state.mapSelectedId);
  if (!selected) return;
  const selectedPosition = getMapPosition(selected, state.cameras.findIndex((camera) => camera.id === selected.id), state.cameras.length);
  els.mapCameraDetail.innerHTML = `
    <article class="route-summary">
      <div class="camera-name">${selected.name}</div>
      <div class="camera-meta">${selected.id} - ${selected.area} - ${selected.direction}</div>
      <div class="detail-row"><strong>Position</strong><span>X ${selectedPosition.x}, Y ${selectedPosition.y}, Facing ${selectedPosition.facing} deg</span></div>
      <div class="detail-row"><strong>Route</strong><span>Prev ${(selected.previous || []).length}, Next ${(selected.next || []).length}, Near ${(selected.related || []).length}</span></div>
      <div class="form-actions">
        <button id="mapOpenCameraBtn" type="button">Open</button>
        <button id="mapRouteCameraBtn" class="secondary" type="button">Route</button>
      </div>
    </article>
  `;

  document.getElementById("mapOpenCameraBtn").addEventListener("click", () => {
    setSelected(selected.id);
    addToGrid(selected.id);
    setView("operator", { push: true });
  });
  document.getElementById("mapRouteCameraBtn").addEventListener("click", () => {
    state.routeSelectedId = selected.id;
    setView("routes", { push: true });
    renderRoutes();
  });
}

function saveMapPosition() {
  const id = els.mapCameraInput.value;
  if (!byId.has(id)) return;
  state.mapSelectedId = id;
  state.mapPositions[id] = {
    x: Math.max(4, Math.min(96, Number(els.mapXInput.value || 50))),
    y: Math.max(4, Math.min(96, Number(els.mapYInput.value || 50))),
    facing: Number(els.mapFacingInput.value || 0)
  };
  persist();
  renderMap();
}

function autoPlaceMap() {
  const visibleCameras = state.cameras.filter((camera) => state.mapArea === "All" || camera.area === state.mapArea);
  visibleCameras.forEach((camera, index) => {
    state.mapPositions[camera.id] = defaultMapPosition(camera, index, visibleCameras.length);
  });
  persist();
  renderMap();
}

function todayValue() {
  return formatDate(new Date());
}

function padTimePart(value) {
  return String(value).padStart(2, "0");
}

function formatDate(date) {
  return `${date.getFullYear()}-${padTimePart(date.getMonth() + 1)}-${padTimePart(date.getDate())}`;
}

function formatTime(date) {
  return `${padTimePart(date.getHours())}:${padTimePart(date.getMinutes())}:${padTimePart(date.getSeconds())}`;
}

function formatDateTime(date = new Date()) {
  return `${formatDate(date)} ${formatTime(date)}`;
}

let playbackActiveDay = startOfDay(parseStoredDateTime(state.playbackRangeStart));
let playbackCursorDate = clampDateToDay(parseStoredDateTime(state.playbackRangeStart), playbackActiveDay);
let playbackPlaying = false;
let playbackTimerHandle = null;
const playbackSpeeds = [1, 2, 4, 8];
let playbackSpeedIndex = 0;
let playbackScrubbing = false;

function parseStoredDateTime(value) {
  if (!value) return new Date();
  const [datePart, timePart] = value.split(" ");
  const [y, m, d] = (datePart || todayValue()).split("-").map(Number);
  const [hh, mm, ss] = (timePart || "00:00:00").split(":").map(Number);
  return new Date(y, (m || 1) - 1, d || 1, hh || 0, mm || 0, ss || 0);
}

function startOfDay(date) {
  return new Date(date.getFullYear(), date.getMonth(), date.getDate());
}

function clampDateToDay(date, day) {
  const dayStart = startOfDay(day).getTime();
  const dayEnd = dayStart + 86400000;
  const ms = Math.min(Math.max(date.getTime(), dayStart), dayEnd - 1000);
  return new Date(ms);
}

function shortDateTime(date) {
  return `${padTimePart(date.getMonth() + 1)}/${padTimePart(date.getDate())} ${formatTime(date)}`;
}

// --- Playback camera tree (mirrors the Live View resource tree) --------

function renderPlaybackTree() {
  const filtered = state.cameras.filter((camera) => {
    const query = state.playbackQuery.trim().toLowerCase();
    if (!query) return true;
    const haystack = [camera.id, camera.name, camera.area, camera.floor, cameraDeviceName(camera), ...(camera.tags || [])]
      .join(" ")
      .toLowerCase();
    return haystack.includes(query);
  });

  els.pbCameraCount.textContent = `${filtered.length} shown`;

  if (!filtered.length) {
    els.pbCameraList.innerHTML = `<div class="empty-state">No matching cameras.</div>`;
    return;
  }

  const grouped = groupSectionsForCameras(filtered);
  const groupHtml = grouped.sections
    .map(({ group, members }) => {
      const isOpen = Boolean(state.playbackQuery);
      return `
      <details class="camera-group-section" ${isOpen ? "open" : ""}>
        <summary>
          <span class="camera-group-title"><span class="tree-icon">&#128193;</span><span class="camera-group-name">${escapeHtml(group.name)}</span>${group.system ? ` <span class="group-badge-system">device</span>` : ""}</span>
          <span class="camera-group-actions">
            <span class="camera-group-count">${members.length}</span>
            <button class="tree-action-btn" data-open-pb-list-group="${escapeHtml(group.id)}" type="button" title="Open all cameras in group for playback" aria-label="Open all cameras in ${escapeHtml(group.name)} for playback">&#9654;</button>
          </span>
        </summary>
        <div class="camera-group-members">${members.map(playbackCameraRowHTML).join("")}</div>
      </details>
    `;
    })
    .join("");
  const ungroupedHtml = grouped.ungrouped.length
    ? `
      <details class="camera-group-section" open>
        <summary>
          <span class="camera-group-title"><span class="camera-group-name">Ungrouped</span></span>
          <span class="camera-group-count">${grouped.ungrouped.length}</span>
        </summary>
        <div class="camera-group-members">${grouped.ungrouped.map(playbackCameraRowHTML).join("")}</div>
      </details>
    `
    : "";
  els.pbCameraList.innerHTML = groupHtml + ungroupedHtml;

  els.pbCameraList.querySelectorAll("[data-open-pb-list-group]").forEach((button) => {
    button.addEventListener("click", (event) => {
      event.preventDefault();
      event.stopPropagation();
      openPlaybackGroup(button.dataset.openPbListGroup);
    });
  });
  els.pbCameraList.querySelectorAll("[data-pb-camera-row]").forEach((row) => {
    bindClickAndDoubleClick(row, {
      onClick: () => addToPlaybackGrid(row.dataset.pbCameraRow),
      onDoubleClick: () => {
        const id = row.dataset.pbCameraRow;
        addToPlaybackGrid(id);
        maximizedPlaybackCameraId = maximizedPlaybackCameraId === id ? "" : id;
        renderPlaybackTileGrid();
      }
    });
    row.addEventListener("keydown", (event) => {
      if (event.key === "Enter") addToPlaybackGrid(row.dataset.pbCameraRow);
    });
  });
}

function playbackCameraRowHTML(camera) {
  const cameraTags = (camera.tags || []).slice(0, 3);
  return `
    <article class="camera-row ${camera.id === state.playbackSelectedId ? "active" : ""}" data-pb-camera-row="${escapeHtml(camera.id)}" tabindex="0">
      <div class="camera-row-main">
        <div class="camera-row-header">
          <div class="camera-name"><span class="tree-icon">&#9673;</span>${escapeHtml(camera.name)}</div>
          <span class="status ${escapeHtml(camera.status)}">${escapeHtml(camera.status)}</span>
        </div>
        <div class="camera-meta">${escapeHtml(camera.id)} - ${escapeHtml(camera.area)} - ${escapeHtml(cameraDeviceName(camera))} CH-${escapeHtml(camera.channel)}</div>
        <div class="tag-line">${cameraTags.map((tag) => `<span class="tag">${escapeHtml(tag)}</span>`).join("")}</div>
      </div>
    </article>
  `;
}

// --- Playback grid (reuses the simple-live-grid tile look from Live View) ---

function addToPlaybackGrid(ids) {
  const requested = Array.isArray(ids) ? ids : [ids];
  const admitted = [];
  requested.forEach((id) => {
    if (byId.has(id) && !state.playbackOpenIds.includes(id)) {
      if (!canAdmitCamera("playback", id)) return;
      state.playbackOpenIds.push(id);
      admitted.push(id);
    } else if (byId.has(id) && state.playbackOpenIds.includes(id)) {
      admitted.push(id);
    }
  });
  if (state.playbackOpenIds.length > state.playbackGridDivision) {
    state.playbackGridDivision = Math.min(16, state.playbackOpenIds.length);
  }
  if (admitted.length) {
    state.playbackSelectedId = admitted[admitted.length - 1];
  }
  persist();
  refreshWorkspaceResourceUi();
  renderPlaybackTileGrid();
  renderPlaybackTree();
}

function openPlaybackGroup(id) {
  const group = state.groups.find((item) => item.id === id);
  if (!group) return;
  const cameraIds = workspaceAdmissionIds("playback", (group.cameraIds || []).slice(0, 16));
  maximizedPlaybackCameraId = "";
  state.playbackOpenIds = cameraIds;
  const requestedDivision = Number(group.grid || state.playbackGridDivision || cameraIds.length || 4);
  state.playbackGridDivision = Math.max(
    cameraIds.length || 1,
    Math.min(16, Math.round(requestedDivision))
  );
  state.playbackSelectedId = cameraIds[0] || "";
  persist();
  refreshWorkspaceResourceUi();
  renderPlaybackTileGrid();
  renderPlaybackTree();
}

function removeFromPlaybackGrid(id) {
  state.playbackOpenIds = state.playbackOpenIds.filter((existingId) => existingId !== id);
  if (state.playbackSelectedId === id) state.playbackSelectedId = "";
  if (maximizedPlaybackCameraId === id) maximizedPlaybackCameraId = "";
  persist();
  refreshWorkspaceResourceUi();
  renderPlaybackTileGrid();
  renderPlaybackTree();
}

function setPlaybackDivision(division) {
  maximizedPlaybackCameraId = "";
  state.playbackGridDivision = Math.max(1, Math.min(16, Math.round(Number(division) || 4)));
  persist();
  els.pbLayoutPopover?.querySelectorAll("[data-pb-division]").forEach((button) => {
    button.classList.toggle("active", Number(button.dataset.pbDivision) === state.playbackGridDivision);
  });
  if (playbackSpatialCanvas) playbackSpatialCanvas.setDivision(state.playbackGridDivision);
  renderPlaybackTileGrid();
}

function renderPlaybackTileGrid() {
  if (playbackSpatialCanvas) {
    playbackSpatialCanvas.destroy();
    playbackSpatialCanvas = null;
  }

  const openCameras = state.playbackOpenIds
    .map((id) => byId.get(id))
    .filter(Boolean);
  const maximizedCamera = byId.get(maximizedPlaybackCameraId);
  if (maximizedPlaybackCameraId && !maximizedCamera) maximizedPlaybackCameraId = "";
  const slotCount = maximizedCamera ? 1 : Math.max(1, state.playbackGridDivision);
  const camerasToShow = maximizedCamera ? [maximizedCamera] : openCameras.slice(0, slotCount);

  if (state.useDraggableCanvases && els.playbackSpatialCanvasEl) {
    els.playbackTileGrid.style.display = "none";
    els.playbackSpatialCanvasEl.style.display = "block";
    playbackSpatialCanvas = new window.SpatialCanvas(els.playbackSpatialCanvasEl, {
      onCameraSelect: (camera) => {
        state.playbackSelectedId = camera.id;
        persist();
        playbackSpatialCanvas?.focusCamera(camera.id, { center: false });
        renderPlaybackTree();
      },
      onCameraDoubleClick: (camera) => {
        maximizedPlaybackCameraId = maximizedPlaybackCameraId === camera.id ? "" : camera.id;
        state.playbackSelectedId = camera.id;
        persist();
        renderPlaybackTileGrid();
      },
      onTierChange: () => {},
      maxActiveStreams: effectiveResourceLimit(),
      division: slotCount
    });
    const laidOut = window.SpatialCanvasLayout.layoutGrid(camerasToShow, slotCount);
    playbackSpatialCanvas.setCameras(laidOut);
    if (state.playbackSelectedId) {
      playbackSpatialCanvas.focusCamera(state.playbackSelectedId, { center: false });
    }
    return;
  }

  if (els.playbackSpatialCanvasEl) els.playbackSpatialCanvasEl.style.display = "none";
  els.playbackTileGrid.className = `simple-live-grid playback-tile-grid ${slotCount === 1 ? "focused-feed-grid" : ""}`;
  els.playbackTileGrid.style.display = "grid";
  applyGridLayout(els.playbackTileGrid, slotCount);

  const tileHtml = camerasToShow.map((camera) => {
    const tier = resolveTier({
      tileCount: camerasToShow.length,
      isFocused: camera.id === state.playbackSelectedId || camerasToShow.length === 1,
      isTracking: false,
      paneContext: "playback",
      isVisible: true
    });
    return `
      <article class="simple-video-tile ${camera.id === state.playbackSelectedId ? "active" : ""}" data-pb-tile="${camera.id}">
        <div class="tile-header">
          <strong>${camera.name}</strong>
          <span class="status ${camera.status}">${tier}</span>
        </div>
        <div class="simple-video-body">
          <div>
            <strong>Playback bridge pending</strong>
            ${mediaStatusText(camera, "playback")}${mediaPolicyLine(tier)}
          </div>
        </div>
        <div class="tile-footer">
          <span>${camera.id}</span>
          <span>${camera.area}</span>
          <span class="pb-tile-actions">
            <button class="secondary" data-pb-report="${camera.id}" type="button">Report</button>
            <button class="danger" data-pb-remove="${camera.id}" type="button">X</button>
          </span>
        </div>
      </article>
    `;
  });
  const emptySlotHtml = `<div class="simple-video-tile empty-slot">Empty channel</div>`;
  const emptyCount = Math.max(0, slotCount - camerasToShow.length);
  els.playbackTileGrid.innerHTML = tileHtml.join("") + emptySlotHtml.repeat(emptyCount);

  els.playbackTileGrid.querySelectorAll("[data-pb-tile]").forEach((tile) => {
    bindClickAndDoubleClick(tile, {
      ignore: (event) => Boolean(event.target.closest("[data-pb-report], [data-pb-remove]")),
      onClick: () => {
        state.playbackSelectedId = tile.dataset.pbTile;
        persist();
        renderPlaybackTileGrid();
      },
      onDoubleClick: () => {
        const id = tile.dataset.pbTile;
        maximizedPlaybackCameraId = maximizedPlaybackCameraId === id ? "" : id;
        state.playbackSelectedId = id;
        persist();
        renderPlaybackTileGrid();
      }
    });
  });
  els.playbackTileGrid.querySelectorAll("[data-pb-remove]").forEach((button) => {
    button.addEventListener("click", (event) => {
      event.stopPropagation();
      removeFromPlaybackGrid(button.dataset.pbRemove);
    });
  });
  els.playbackTileGrid.querySelectorAll("[data-pb-report]").forEach((button) => {
    button.addEventListener("click", (event) => {
      event.stopPropagation();
      preparePlaybackReport(button.dataset.pbReport);
    });
  });
}

function preparePlaybackReport(cameraId) {
  const camera = byId.get(cameraId);
  if (!camera) return;
  const note = window.prompt("Add a note for this playback report:", "Playback review");
  if (note === null) return;
  const incident = saveIncident({
    time: `${formatDate(playbackActiveDay)} ${formatTime(playbackCursorDate)}`,
    source: "Playback",
    cameraId: camera.id,
    cameraName: camera.name,
    area: camera.area,
    device: `${cameraDeviceName(camera)}, Channel ${camera.channel}`,
    type: "Playback reference",
    note: note.trim() || "Playback review",
    status: "Ready"
  });
  window.alert(`Saved playback reference to incidents:\n\n${incidentText(incident)}`);
}

// --- Playback time range popover (start/end date+time search window) ----

function formatPlaybackRangeLabel() {
  const start = parseStoredDateTime(state.playbackRangeStart);
  const end = parseStoredDateTime(state.playbackRangeEnd);
  return `${shortDateTime(start)} - ${shortDateTime(end)}`;
}

function openPlaybackRangePopover() {
  const start = parseStoredDateTime(state.playbackRangeStart);
  const end = parseStoredDateTime(state.playbackRangeEnd);
  els.pbRangeStartDate.value = formatDate(start);
  els.pbRangeStartTime.value = formatTime(start);
  els.pbRangeEndDate.value = formatDate(end);
  els.pbRangeEndTime.value = formatTime(end);
  els.pbRangePopover.hidden = false;
}

function applyPlaybackRange() {
  const start = combineDateAndTime(els.pbRangeStartDate.value, els.pbRangeStartTime.value, "00:00:00");
  const end = combineDateAndTime(els.pbRangeEndDate.value, els.pbRangeEndTime.value, "23:59:59");
  if (end.getTime() <= start.getTime()) {
    window.alert("End date/time must be after the start date/time.");
    return;
  }
  state.playbackRangeStart = formatDateTime(start);
  state.playbackRangeEnd = formatDateTime(end);
  persist();
  playbackActiveDay = startOfDay(start);
  playbackCursorDate = clampDateToDay(start, playbackActiveDay);
  stopPlaybackPlay();
  els.pbRangePopover.hidden = true;
  renderPlaybackScrubber();
}

function combineDateAndTime(dateValue, timeValue, fallbackTime) {
  const datePart = dateValue || todayValue();
  const timePart = timeValue || fallbackTime;
  return parseStoredDateTime(`${datePart} ${timePart}`);
}

// --- Playback timeline scrubber (desktop-VMS-style 24h bar with playhead) ------

function buildPlaybackTicks() {
  const ticks = [];
  for (let hour = 0; hour <= 22; hour += 2) {
    ticks.push(`<div class="pb-tick"><span>${padTimePart(hour)}:00</span></div>`);
  }
  els.pbTicks.innerHTML = ticks.join("");
}

function renderPlaybackScrubber() {
  const dayStart = startOfDay(playbackActiveDay);
  const dayEnd = new Date(dayStart.getTime() + 86400000);
  const rangeStart = parseStoredDateTime(state.playbackRangeStart);
  const rangeEnd = parseStoredDateTime(state.playbackRangeEnd);

  const clippedStart = new Date(Math.max(dayStart.getTime(), rangeStart.getTime()));
  const clippedEnd = new Date(Math.min(dayEnd.getTime(), rangeEnd.getTime()));
  if (clippedEnd.getTime() > clippedStart.getTime()) {
    const leftPct = ((clippedStart.getTime() - dayStart.getTime()) / 86400000) * 100;
    const widthPct = ((clippedEnd.getTime() - clippedStart.getTime()) / 86400000) * 100;
    els.pbTrackAvailable.style.display = "block";
    els.pbTrackAvailable.style.left = `${leftPct}%`;
    els.pbTrackAvailable.style.width = `${widthPct}%`;
  } else {
    els.pbTrackAvailable.style.display = "none";
  }

  const cursorPct = ((playbackCursorDate.getTime() - dayStart.getTime()) / 86400000) * 100;
  els.pbPlayhead.style.left = `${Math.min(100, Math.max(0, cursorPct))}%`;

  els.pbCurrentTime.textContent = formatTime(playbackCursorDate);
  els.pbDateLabel.textContent = `${padTimePart(playbackActiveDay.getMonth() + 1)}/${padTimePart(playbackActiveDay.getDate())}`;
  els.pbRangeLabel.textContent = formatPlaybackRangeLabel();
  els.pbSpeedBtn.textContent = `${playbackSpeeds[playbackSpeedIndex]}x`;
  els.pbPlayBtn.innerHTML = playbackPlaying ? "&#10074;&#10074;" : "&#9654;";
}

function seekPlaybackToClientX(clientX) {
  const rect = els.pbTrack.getBoundingClientRect();
  const percent = Math.min(1, Math.max(0, (clientX - rect.left) / rect.width));
  const dayStart = startOfDay(playbackActiveDay);
  playbackCursorDate = new Date(dayStart.getTime() + Math.round(percent * 86400000));
  renderPlaybackScrubber();
}

function stepPlayback(deltaSeconds) {
  const dayStart = startOfDay(playbackActiveDay);
  const dayEnd = new Date(dayStart.getTime() + 86400000 - 1000);
  const next = new Date(playbackCursorDate.getTime() + deltaSeconds * 1000);
  playbackCursorDate = next.getTime() < dayStart.getTime() ? dayStart : next.getTime() > dayEnd.getTime() ? dayEnd : next;
  renderPlaybackScrubber();
}

function stopPlaybackPlay() {
  playbackPlaying = false;
  if (playbackTimerHandle) {
    clearInterval(playbackTimerHandle);
    playbackTimerHandle = null;
  }
}

function togglePlaybackPlay() {
  if (playbackPlaying) {
    stopPlaybackPlay();
    renderPlaybackScrubber();
    return;
  }
  playbackPlaying = true;
  playbackTimerHandle = setInterval(() => {
    const dayStart = startOfDay(playbackActiveDay);
    const dayEnd = new Date(dayStart.getTime() + 86400000);
    const next = new Date(playbackCursorDate.getTime() + playbackSpeeds[playbackSpeedIndex] * 1000);
    if (next.getTime() >= dayEnd.getTime()) {
      playbackCursorDate = new Date(dayEnd.getTime() - 1000);
      stopPlaybackPlay();
    } else {
      playbackCursorDate = next;
    }
    renderPlaybackScrubber();
  }, 1000);
  renderPlaybackScrubber();
}

function changePlaybackDay(deltaDays) {
  stopPlaybackPlay();
  const timeOfDayMs = playbackCursorDate.getTime() - startOfDay(playbackActiveDay).getTime();
  playbackActiveDay = new Date(startOfDay(playbackActiveDay).getTime() + deltaDays * 86400000);
  playbackCursorDate = new Date(startOfDay(playbackActiveDay).getTime() + timeOfDayMs);
  renderPlaybackScrubber();
}

function incidentText(incident) {

  return [
    `Time: ${incident.time}`,
    `Source: ${incident.source}`,
    `Location: ${incident.area}`,
    `Camera: ${incident.cameraName} (${incident.cameraId})`,
    `Device: ${incident.device}`,
    `Issue: ${incident.type}`,
    `Note: ${incident.note}`,
    `Status: ${incident.status}`
  ].join("\n");
}

function saveIncident(incident) {
  const record = {
    id: incident.id || `inc-${Date.now()}`,
    time: incident.time || formatDateTime(),
    source: incident.source || "Live",
    cameraId: incident.cameraId,
    cameraName: incident.cameraName,
    area: incident.area,
    device: incident.device,
    type: incident.type || "Observation",
    note: incident.note || "Details pending.",
    status: incident.status || "Ready"
  };

  state.incidents.unshift(record);
  state.selectedIncidentId = record.id;
  state.incidents = state.incidents.slice(0, 200);
  persist();
  renderIncidents();
  return record;
}

function setIncidentStatus(id, status) {
  state.incidents = state.incidents.map((incident) => (incident.id === id ? { ...incident, status } : incident));
  state.selectedIncidentId = id;
  persist();
  renderIncidents();
}

function deleteIncident(id) {
  if (!window.confirm("Delete this incident report?")) return;
  state.incidents = state.incidents.filter((incident) => incident.id !== id);
  if (state.selectedIncidentId === id) {
    state.selectedIncidentId = state.incidents[0]?.id || "";
  }
  persist();
  renderIncidents();
}

function selectIncident(id) {
  state.selectedIncidentId = id;
  persist();
  renderIncidents();
}

function renderIncidents() {
  const query = state.incidentQuery.trim().toLowerCase();
  const filtered = state.incidents.filter((incident) => {
    const haystack = [incident.cameraName, incident.cameraId, incident.area, incident.type, incident.note, incident.source, incident.status]
      .join(" ")
      .toLowerCase();
    return (state.incidentStatus === "All" || incident.status === state.incidentStatus) && (!query || haystack.includes(query));
  });

  els.incidentStatusFilter.value = state.incidentStatus;
  els.incidentSearchInput.value = state.incidentQuery;
  els.incidentCount.textContent = `${filtered.length} shown`;

  const totals = ["Draft", "Ready", "Sent", "Closed"].map((status) => ({
    status,
    count: state.incidents.filter((incident) => incident.status === status).length
  }));
  els.incidentStats.innerHTML = totals
    .map((item) => `<div class="metric"><span>${item.status}</span><strong>${item.count}</strong></div>`)
    .join("");

  els.incidentList.innerHTML = "";
  if (!filtered.length) {
    els.incidentList.innerHTML = `<div class="empty-state">No incident reports match this filter.</div>`;
  }

  filtered.forEach((incident) => {
    const card = document.createElement("article");
    card.className = `incident-card ${incident.id === state.selectedIncidentId ? "active" : ""}`;
    card.innerHTML = `
      <div class="incident-card-header">
        <div>
          <div class="camera-name">${incident.type}</div>
          <div class="camera-meta">${incident.time} - ${incident.cameraName} - ${incident.area}</div>
        </div>
        <span class="status ${incident.status.toLowerCase()}">${incident.status}</span>
      </div>
      <div class="incident-body">${incident.note}</div>
      <div class="device-actions">
        <button class="secondary" data-select-incident="${incident.id}" type="button">Preview</button>
        <button data-status-incident="${incident.id}" data-incident-status="Sent" type="button">Sent</button>
        <button class="secondary" data-status-incident="${incident.id}" data-incident-status="Closed" type="button">Close</button>
        <button class="danger" data-delete-incident="${incident.id}" type="button">Delete</button>
      </div>
    `;
    els.incidentList.appendChild(card);
  });

  const selected = state.incidents.find((incident) => incident.id === state.selectedIncidentId) || filtered[0];
  els.incidentPreview.innerHTML = selected
    ? `
      <article class="route-summary">
        <div class="camera-name">WhatsApp Report Text</div>
        <div class="camera-meta">${selected.source} - ${selected.status}</div>
        <pre class="report-output">${incidentText(selected)}</pre>
        <div class="form-actions">
          <button id="incidentUseCameraBtn" type="button">Open Camera</button>
          <button id="incidentMarkSentBtn" class="secondary" type="button">Mark Sent</button>
        </div>
      </article>
    `
    : `<div class="empty-state">Select or create an incident report.</div>`;

  if (selected) {
    document.getElementById("incidentUseCameraBtn").addEventListener("click", () => {
      setSelected(selected.cameraId);
      addToGrid(selected.cameraId);
      setView("operator", { push: true });
    });
    document.getElementById("incidentMarkSentBtn").addEventListener("click", () => setIncidentStatus(selected.id, "Sent"));
  }

  document.querySelectorAll("[data-select-incident]").forEach((button) => {
    button.addEventListener("click", () => selectIncident(button.dataset.selectIncident));
  });
  document.querySelectorAll("[data-status-incident]").forEach((button) => {
    button.addEventListener("click", () => setIncidentStatus(button.dataset.statusIncident, button.dataset.incidentStatus));
  });
  document.querySelectorAll("[data-delete-incident]").forEach((button) => {
    button.addEventListener("click", () => deleteIncident(button.dataset.deleteIncident));
  });
}

function renderGroupDeviceFilterOptions() {
  if (!els.groupDeviceFilterInput) return;
  const current = els.groupDeviceFilterInput.value || "All";
  els.groupDeviceFilterInput.innerHTML = [
    `<option value="All">All devices</option>`,
    ...state.devices.map((device) => `<option value="${device.id}">${device.name} (${device.type})</option>`),
    `<option value="__unassigned__">Unassigned / manual cameras</option>`
  ].join("");
  if (Array.from(els.groupDeviceFilterInput.options).some((option) => option.value === current)) {
    els.groupDeviceFilterInput.value = current;
  }
}

function groupFormAvailableCameras() {
  const filterValue = els.groupDeviceFilterInput ? els.groupDeviceFilterInput.value : "All";
  if (filterValue === "All") return state.cameras;
  if (filterValue === "__unassigned__") {
    return state.cameras.filter((camera) => !cameraDevice(camera));
  }
  const device = state.devices.find((item) => item.id === filterValue);
  return device ? state.cameras.filter((camera) => cameraBelongsToDevice(camera, device)) : [];
}

function renderGroupCameraOptions() {
  const members = new Set(groupFormMemberIds);
  els.groupCameraSelect.innerHTML = groupFormAvailableCameras()
    .filter((camera) => !members.has(camera.id))
    .map((camera) => `<option value="${camera.id}">${camera.name} - ${cameraDeviceName(camera)} CH-${camera.channel}</option>`)
    .join("");
}

function renderGroupMembersList() {
  if (!els.groupMembersList) return;
  const cameras = groupFormMemberIds.map((id) => byId.get(id)).filter(Boolean);
  if (!cameras.length) {
    els.groupMembersList.innerHTML = `<div class="empty-state">No cameras in this group yet.</div>`;
    return;
  }
  els.groupMembersList.innerHTML = cameras
    .map((camera) => `
      <span class="group-member-chip">
        ${camera.name}
        <button data-remove-group-member="${camera.id}" type="button">x</button>
      </span>
    `)
    .join("");
  els.groupMembersList.querySelectorAll("[data-remove-group-member]").forEach((button) => {
    button.addEventListener("click", () => {
      groupFormMemberIds = groupFormMemberIds.filter((id) => id !== button.dataset.removeGroupMember);
      renderGroupCameraOptions();
      renderGroupMembersList();
    });
  });
}

function resetGroupForm() {
  els.groupForm.reset();
  els.groupIdInput.value = "";
  els.groupGridInput.value = "4";
  groupFormMemberIds = [];
  renderGroupDeviceFilterOptions();
  renderGroupCameraOptions();
  renderGroupMembersList();
}

function loadGroupIntoForm(id) {
  const group = state.groups.find((item) => item.id === id);
  if (!group) return;
  if (group.system) {
    window.alert("This is an auto-assigned device group. Edit the device to change its channels.");
    return;
  }
  state.selectedGroupId = group.id;
  els.groupIdInput.value = group.id;
  els.groupNameInput.value = group.name;
  els.groupPurposeInput.value = group.purpose || "";
  els.groupGridInput.value = String(group.grid || 4);
  els.groupNotesInput.value = group.notes || "";
  groupFormMemberIds = [...(group.cameraIds || [])];
  renderGroupDeviceFilterOptions();
  renderGroupCameraOptions();
  renderGroupMembersList();
  persist();
  renderGroups();
}

function saveGroup(event) {
  event.preventDefault();
  if (!requireInventoryMutation()) return;
  const group = {
    id: els.groupIdInput.value || `grp-${Date.now()}`,
    name: els.groupNameInput.value.trim(),
    purpose: els.groupPurposeInput.value.trim(),
    grid: Number(els.groupGridInput.value || 4),
    cameraIds: groupFormMemberIds.filter((id) => byId.has(id)),
    notes: els.groupNotesInput.value.trim(),
    system: false
  };

  const index = state.groups.findIndex((item) => item.id === group.id);
  if (index >= 0) {
    state.groups[index] = group;
  } else {
    state.groups.unshift(group);
  }
  state.selectedGroupId = group.id;
  persist();
  resetGroupForm();
  renderGroups();
}

function deleteGroup(id) {
  if (!requireInventoryMutation()) return;
  const group = state.groups.find((item) => item.id === id);
  if (group?.system) {
    window.alert("This is an auto-assigned device group. Delete or edit the device to change it.");
    return;
  }
  if (!window.confirm("Delete this camera group?")) return;
  state.groups = state.groups.filter((group) => group.id !== id);
  if (state.selectedGroupId === id) {
    state.selectedGroupId = state.groups[0]?.id || "";
  }
  persist();
  renderGroups();
}

function openGroup(id) {
  const group = state.groups.find((item) => item.id === id);
  if (!group) return;
  maximizedLiveCameraId = "";
  state.openIds = workspaceAdmissionIds("live", group.cameraIds || []);
  const requestedDivision = Number(group.grid || state.gridDivision || state.openIds.length || 4);
  state.gridDivision = Math.max(state.openIds.length || 1, Math.min(64, Math.round(requestedDivision)));
  state.selectedGroupId = group.id;
  if (state.openIds[0]) {
    state.selectedId = state.openIds[0];
  }
  frameLiveViewport(state.openIds);
  persist();
  setView("operator", { push: true });
  render();
}

function renderGroups() {
  renderGroupDeviceFilterOptions();
  renderGroupCameraOptions();
  renderGroupMembersList();
  els.groupCount.textContent = `${state.groups.length} saved`;
  els.groupList.innerHTML = "";

  if (!state.groups.length) {
    els.groupList.innerHTML = `<div class="empty-state">No camera groups saved yet.</div>`;
    return;
  }

  state.groups.forEach((group) => {
    const cameras = (group.cameraIds || []).map((id) => byId.get(id)).filter(Boolean);
    const card = document.createElement("article");
    card.className = `group-card ${group.id === state.selectedGroupId ? "active" : ""}`;
    card.innerHTML = `
      <div class="incident-card-header">
        <div>
          <div class="camera-name">${group.name}${group.system ? ` <span class="group-badge-system">device</span>` : ""}</div>
          <div class="camera-meta">${group.purpose || "No purpose set"} - ${cameras.length} cameras - ${group.grid || 4} grid</div>
        </div>
        <span class="status ready">Layout</span>
      </div>
      <div class="tag-line">${cameras.map((camera) => `<span class="tag">${camera.name}</span>`).join("") || `<span class="tag">No cameras</span>`}</div>
      <div class="incident-body">${group.notes || "No notes added."}</div>
      <div class="device-actions">
        <button data-open-group="${group.id}" type="button">Open</button>
        ${group.system ? `<span class="camera-meta">Auto-managed from device</span>` : `
          <button class="secondary" data-edit-group="${group.id}" type="button">Edit</button>
          <button class="danger" data-delete-group="${group.id}" type="button">Delete</button>
        `}
      </div>
    `;
    els.groupList.appendChild(card);
  });

  document.querySelectorAll("[data-open-group]").forEach((button) => {
    button.addEventListener("click", () => openGroup(button.dataset.openGroup));
  });
  document.querySelectorAll("[data-edit-group]").forEach((button) => {
    button.addEventListener("click", () => loadGroupIntoForm(button.dataset.editGroup));
  });
  document.querySelectorAll("[data-delete-group]").forEach((button) => {
    button.addEventListener("click", () => deleteGroup(button.dataset.deleteGroup));
  });
}

function renderUserGroupOptions(selectedIds = []) {
  const selected = new Set(selectedIds);
  els.userGroupSelect.innerHTML = state.groups
    .map((group) => `<option value="${group.id}" ${selected.has(group.id) ? "selected" : ""}>${group.name}</option>`)
    .join("");
}

function setMultiSelectValues(select, values) {
  const selected = new Set(values);
  Array.from(select.options).forEach((option) => {
    option.selected = selected.has(option.value);
  });
}

function resetUserForm() {
  els.userForm.reset();
  els.userIdInput.value = "";
  els.userRoleInput.value = INITIAL_ROLE;
  els.userStatusInput.value = "Active";
  renderUserGroupOptions();
  setMultiSelectValues(els.userPermissionSelect, FEATURE_CAPABILITIES);
}

function loadUserIntoForm(id) {
  const user = state.users.find((item) => item.id === id);
  if (!user) return;
  state.selectedUserId = user.id;
  els.userIdInput.value = user.id;
  els.userNameInput.value = user.name;
  els.usernameInput.value = user.username;
  els.userRoleInput.value = user.role;
  els.userStatusInput.value = user.status;
  els.userNotesInput.value = user.notes || "";
  renderUserGroupOptions(user.groupIds || []);
  setMultiSelectValues(els.userPermissionSelect, user.permissions || []);
  persist();
  renderUsers();
}

function saveUser(event) {
  event.preventDefault();
  const user = {
    id: els.userIdInput.value || `usr-${Date.now()}`,
    name: els.userNameInput.value.trim(),
    username: els.usernameInput.value.trim(),
    role: INITIAL_ROLE,
    status: els.userStatusInput.value,
    groupIds: Array.from(els.userGroupSelect.selectedOptions).map((option) => option.value),
    permissions: [...FEATURE_CAPABILITIES],
    notes: els.userNotesInput.value.trim()
  };

  const duplicate = state.users.some((item) => item.username === user.username && item.id !== user.id);
  if (duplicate) {
    window.alert("A user with this username already exists.");
    return;
  }

  const index = state.users.findIndex((item) => item.id === user.id);
  if (index >= 0) {
    state.users[index] = user;
  } else {
    state.users.unshift(user);
  }
  state.selectedUserId = user.id;
  persist();
  resetUserForm();
  renderUsers();
}

function setUserStatus(id, status) {
  state.users = state.users.map((user) => (user.id === id ? { ...user, status } : user));
  state.selectedUserId = id;
  persist();
  renderUsers();
}

function deleteUser(id) {
  if (!window.confirm("Delete this user record?")) return;
  state.users = state.users.filter((user) => user.id !== id);
  if (state.selectedUserId === id) {
    state.selectedUserId = state.users[0]?.id || "";
  }
  persist();
  renderUsers();
}

function renderUsers() {
  renderUserGroupOptions(Array.from(els.userGroupSelect.selectedOptions || []).map((option) => option.value));
  els.userCount.textContent = `${state.users.length} administrator account${state.users.length === 1 ? "" : "s"}`;
  els.userList.innerHTML = "";

  if (!state.users.length) {
    els.userList.innerHTML = `<div class="empty-state">No users created yet.</div>`;
    return;
  }

  state.users.forEach((user) => {
    const groups = (user.groupIds || []).map((id) => state.groups.find((group) => group.id === id)).filter(Boolean);
    const card = document.createElement("article");
    card.className = `user-card ${user.id === state.selectedUserId ? "active" : ""}`;
    card.innerHTML = `
      <div class="incident-card-header">
        <div>
          <div class="camera-name">${user.name}</div>
          <div class="camera-meta">${user.username} - ${user.role} - ${groups.length} groups</div>
        </div>
        <span class="status ${user.status.toLowerCase()}">${user.status}</span>
      </div>
      <div class="tag-line">
        ${(user.permissions || []).map((permission) => `<span class="tag">${permission}</span>`).join("") || `<span class="tag">No permissions</span>`}
      </div>
      <div class="incident-body">
        Groups: ${groups.map((group) => group.name).join(", ") || "No camera groups assigned"}<br>
        ${user.notes || "No notes added."}
      </div>
      <div class="device-actions">
        <button class="secondary" data-edit-user="${user.id}" type="button">Edit</button>
        <button data-status-user="${user.id}" data-user-status="Suspended" type="button">Suspend</button>
        <button class="danger" data-status-user="${user.id}" data-user-status="Revoked" type="button">Revoke</button>
        <button class="secondary" data-status-user="${user.id}" data-user-status="Active" type="button">Activate</button>
        <button class="danger" data-delete-user="${user.id}" type="button">Delete</button>
      </div>
    `;
    els.userList.appendChild(card);
  });

  document.querySelectorAll("[data-edit-user]").forEach((button) => {
    button.addEventListener("click", () => loadUserIntoForm(button.dataset.editUser));
  });
  document.querySelectorAll("[data-status-user]").forEach((button) => {
    button.addEventListener("click", () => setUserStatus(button.dataset.statusUser, button.dataset.userStatus));
  });
  document.querySelectorAll("[data-delete-user]").forEach((button) => {
    button.addEventListener("click", () => deleteUser(button.dataset.deleteUser));
  });
}

function render() {
  refreshCameraIndexes();
  applyTheme();
  applySidebarState();
  els.mediaPolicyToggle.checked = state.showMediaPolicy;
  els.draggableCanvasesToggle.checked = state.useDraggableCanvases;
  syncDisplayModeSwitch(els.draggableCanvasesToggle, state.useDraggableCanvases);
  els.restoreLastCameraGridToggle.checked = state.restoreLastCameraGrid;
  syncStreamingControlState();
  setView(state.view);
  renderWorkspaceTabs("live");
  renderWorkspaceTabs("playback");
  applyWorkspacePresentation();
  renderCameraDeviceOptions(els.cameraDeviceInput.value);
  renderCameraChannelOptions(els.cameraDeviceInput.value, Number(els.cameraChannelSelect?.value || els.cameraChannelInput.value || 1));
  renderChips(els.areaFilters, areas, state.area, (area) => {
    state.area = area;
    render();
  });
  renderChips(els.tagFilters, tags, state.tag, (tag) => {
    state.tag = tag;
    render();
  });
  renderCameraList();
  renderLiveGroups();
  renderStatusBar();
  renderGrid();
  renderMonitorDrawer();
  renderDevices();
  renderCatalog();
  renderRoutes();
  renderMap();
  renderPlaybackTree();
  renderPlaybackTileGrid();
  renderPlaybackScrubber();
  renderIncidents();
  renderGroups();
  renderUsers();
  updateInventoryMutationControls();
}

function syncDisplayModeSwitch(input, draggable) {
  const stateLabel = input?.closest(".modern-switch")?.querySelector(".switch-state");
  if (stateLabel) stateLabel.textContent = draggable ? "Draggable" : "Fixed grid";
}

function syncStreamingControlState() {
  if (els.streamingSettingsSection) {
    els.streamingSettingsSection.hidden = false;
  }
  if (els.gridSettingsBtn) {
    els.gridSettingsBtn.hidden = false;
  }
  if (els.resourceOptimizationMode) {
    els.resourceOptimizationMode.value = state.resourceOptimizationMode;
  }
  const effectiveLimit = effectiveResourceLimit();
  [els.maxActiveStreamsInput, els.maxActiveStreamsInputGrid].forEach((input) => {
    if (!input) return;
    input.value = state.resourceOptimizationMode === "auto" ? effectiveLimit : state.maxActiveStreams;
    input.disabled = state.resourceOptimizationMode === "auto";
    input.title = state.resourceOptimizationMode === "auto"
      ? `Automatic optimization currently allows ${effectiveLimit} camera sessions across all workspaces`
      : "Manual camera-session limit shared by every Live View and Remote Playback tab";
  });
  if (els.resourceBudgetSummary) {
    const used = totalOpenCameraSessions();
    const workspaceCount = state.liveWorkspaces.length + state.playbackWorkspaces.length;
    els.resourceBudgetSummary.textContent =
      `${used} of ${effectiveLimit} camera sessions allocated across ${workspaceCount} workspace tab${workspaceCount === 1 ? "" : "s"}.` +
      (state.resourceLimitNotice ? ` ${state.resourceLimitNotice}` : "");
    els.resourceBudgetSummary.classList.toggle("at-limit", used >= effectiveLimit);
  }
}

function toggleTheme() {
  state.theme = state.theme === "dark" ? "light" : "dark";
  persist();
  applyTheme();
}

if (isVmsTestMode) {
  globalThis.__VMS_TEST_API__ = {
    state,
    FEATURE_CAPABILITIES,
    INITIAL_ROLE,
    applyInventorySnapshot,
    applyDeviceTypeChannelRule,
    cameraBelongsToDevice,
    cameraDevice,
    createInventorySnapshot,
    deviceChannelCount,
    deviceGroupId,
    ensureDeviceGroups,
    inventoryAuthorityStatus: () => ({
      state: inventoryAuthorityState,
      detail: inventoryAuthorityDetail,
      lastSuccessfulAt: inventoryLastSuccessfulAt,
      hasPendingDraft: Boolean(inventoryPendingSnapshot)
    }),
    inventoryFingerprint,
    inventoryMutationAllowed,
    isIpCameraDirectType,
    migrateCameraDeviceLinks,
    normalizeAdministratorUsers,
    normalizeWorkspaceList,
    activeWorkspace,
    syncActiveWorkspaceFromLegacy,
    loadWorkspaceIntoLegacy,
    effectiveResourceLimit,
    totalOpenCameraSessions,
    workspaceAdmissionIds,
    canAdmitCamera,
    enforceGlobalCameraLimit,
    nextWorkspaceId,
    queuePtzAction,
    refreshCameraIndexes,
    setInventoryAuthorityState,
    syncDeviceChannels
  };
} else {
els.inventoryRetryBtn?.addEventListener("click", () => {
  void retryPendingInventorySave();
});
els.statusThemeBtn?.addEventListener("click", toggleTheme);

function clearLiveGrid() {
  maximizedLiveCameraId = "";
  state.openIds = [];
  persist();
  render();
}

els.statusClearBtn?.addEventListener("click", clearLiveGrid);

// --- Live View: Resource/Auto-Switch tabs -------------------------------
els.resourceTabs.forEach((tab) => {
  tab.addEventListener("click", () => {
    els.resourceTabs.forEach((item) => item.classList.toggle("active", item === tab));
    const showResource = tab.dataset.resourceTab === "resource";
    if (els.resourcePane) els.resourcePane.hidden = !showResource;
    if (els.autoSwitchPane) els.autoSwitchPane.hidden = showResource;
  });
});

els.treeSearchInput?.addEventListener("input", (event) => {
  state.query = event.target.value;
  renderCameraList();
});

// --- Live View: floating grid toolbar (window division / close / fullscreen / streaming settings) --
function closeAllGridPopovers() {
  if (els.layoutPopover) els.layoutPopover.hidden = true;
  if (els.streamSettingsPopover) els.streamSettingsPopover.hidden = true;
  if (els.ptzPopover) els.ptzPopover.hidden = true;
}

function setGridDivision(division) {
  maximizedLiveCameraId = "";
  const clamped = Math.max(1, Math.min(64, Math.round(Number(division) || 9)));
  state.gridDivision = clamped;
  persist();
  els.layoutPopover?.querySelectorAll("[data-division]").forEach((button) => {
    button.classList.toggle("active", Number(button.dataset.division) === clamped);
  });
  if (liveSpatialCanvas) liveSpatialCanvas.setDivision(clamped);
  renderGrid();
}

els.gridLayoutBtn?.addEventListener("click", (event) => {
  event.stopPropagation();
  const willShow = els.layoutPopover.hidden;
  closeAllGridPopovers();
  els.layoutPopover.hidden = !willShow;
  if (willShow && els.gridDivisionCustomInput) els.gridDivisionCustomInput.value = state.gridDivision;
});

els.layoutPopover?.querySelectorAll("[data-division]").forEach((button) => {
  button.addEventListener("click", () => {
    setGridDivision(Number(button.dataset.division));
    closeAllGridPopovers();
  });
});

els.gridDivisionCustomBtn?.addEventListener("click", (event) => {
  event.stopPropagation();
  if (!els.gridDivisionCustomInput?.value) return;
  setGridDivision(els.gridDivisionCustomInput.value);
  closeAllGridPopovers();
});
els.layoutPopover?.addEventListener("click", (event) => event.stopPropagation());

els.gridZoomOutBtn?.addEventListener("click", (event) => {
  event.stopPropagation();
  setLiveDigitalZoom(state.liveDigitalZoom - 0.25);
});

els.gridZoomInBtn?.addEventListener("click", (event) => {
  event.stopPropagation();
  setLiveDigitalZoom(state.liveDigitalZoom + 0.25);
});

els.gridPtzBtn?.addEventListener("click", (event) => {
  event.stopPropagation();
  const willShow = els.ptzPopover.hidden;
  closeAllGridPopovers();
  els.ptzPopover.hidden = !willShow;
});

els.ptzPopover?.addEventListener("click", (event) => {
  event.stopPropagation();
  const button = event.target.closest("[data-ptz-action]");
  if (!button) return;
  queuePtzAction(button.dataset.ptzAction);
});

els.gridCloseAllBtn?.addEventListener("click", clearLiveGrid);

els.gridFullscreenBtn?.addEventListener("click", () => {
  if (document.fullscreenElement) {
    document.exitFullscreen();
  } else {
    requestLiveFullscreen();
  }
});

els.gridSettingsBtn?.addEventListener("click", (event) => {
  event.stopPropagation();
  const willShow = els.streamSettingsPopover.hidden;
  closeAllGridPopovers();
  els.streamSettingsPopover.hidden = !willShow;
  if (willShow && els.maxActiveStreamsInputGrid) {
    els.maxActiveStreamsInputGrid.value = effectiveResourceLimit();
  }
});

document.addEventListener("click", () => closeAllGridPopovers());

function applyMaxActiveStreams(value) {
  const parsed = Math.max(1, Math.min(64, Number(value) || 6));
  state.maxActiveStreams = parsed;
  enforceGlobalCameraLimit();
  persist();
  const effectiveLimit = effectiveResourceLimit();
  if (els.maxActiveStreamsInputGrid) els.maxActiveStreamsInputGrid.value = effectiveLimit;
  if (els.maxActiveStreamsInput) els.maxActiveStreamsInput.value = effectiveLimit;
  if (liveSpatialCanvas) liveSpatialCanvas.setMaxActiveStreams(effectiveLimit);
  if (playbackSpatialCanvas) playbackSpatialCanvas.setMaxActiveStreams(effectiveLimit);
  syncStreamingControlState();
  renderWorkspaceTabs("live");
  renderWorkspaceTabs("playback");
  renderGrid();
  renderPlaybackTileGrid();
}

els.maxActiveStreamsInputGrid?.addEventListener("input", (event) => applyMaxActiveStreams(event.target.value));
els.maxActiveStreamsInput?.addEventListener("input", (event) => applyMaxActiveStreams(event.target.value));
els.resourceOptimizationMode?.addEventListener("change", (event) => {
  state.resourceOptimizationMode = event.target.value === "manual" ? "manual" : "auto";
  enforceGlobalCameraLimit();
  persist();
  syncStreamingControlState();
  renderWorkspaceTabs("live");
  renderWorkspaceTabs("playback");
  renderGrid();
  renderPlaybackTileGrid();
});

els.sidebarToggle.addEventListener("click", () => {
  const collapsed = !els.appShell.classList.contains("sidebar-collapsed");
  localStorage.setItem("sidebarCollapsed", String(collapsed));
  applySidebarState();
});

els.navItems.forEach((item) => {
  item.addEventListener("click", () => setView(item.dataset.view, { push: true }));
});

els.dashboardCards.forEach((item) => {
  item.addEventListener("click", () => setView(item.dataset.dashboardView, { push: true }));
});

els.cameraSearch.addEventListener("input", (event) => {
  state.query = event.target.value;
  renderCameraList();
});

els.mediaPolicyToggle.addEventListener("change", (event) => {
  state.showMediaPolicy = event.target.checked;
  persist();
  renderGrid();
  renderPlaybackTileGrid();
});

els.draggableCanvasesToggle.addEventListener("change", (event) => {
  state.useDraggableCanvases = event.target.checked;
  persist();
  syncDisplayModeSwitch(event.target, state.useDraggableCanvases);
  syncStreamingControlState();
  renderGrid();
  renderPlaybackTileGrid();
});

els.restoreLastCameraGridToggle.addEventListener("change", (event) => {
  state.restoreLastCameraGrid = event.target.checked;
  persist();
});

els.createLiveGroupBtn?.addEventListener("click", createLiveGroupFromChecked);
els.addCheckedToGroupBtn?.addEventListener("click", addCheckedToSelectedGroup);

els.deviceForm.addEventListener("submit", saveDevice);
els.deviceType.addEventListener("change", () => applyDeviceTypeChannelRule({ restoreRecorderDefault: true }));

els.deviceAddBtn?.addEventListener("click", () => {
  resetDeviceForm();
  els.deviceName.focus();
});

els.deviceDeleteBtn?.addEventListener("click", () => {
  if (!requireInventoryMutation()) return;
  if (!checkedDeviceIds.size) return;
  const names = state.devices.filter((d) => checkedDeviceIds.has(d.id)).map((d) => d.name).join(", ");
  if (!confirm(`Delete ${checkedDeviceIds.size} device(s)? ${names}`)) return;
  detachDeviceCameras(new Set(checkedDeviceIds));
  state.devices = state.devices.filter((d) => !checkedDeviceIds.has(d.id));
  checkedDeviceIds.clear();
  refreshCameraIndexes();
  persist();
  render();
});

els.deviceRefreshBtn?.addEventListener("click", () => render());

els.deviceExportBtn?.addEventListener("click", () => {
  const header = ["Name", "Connection Type", "Network Parameters", "Device Type", "Vendor", "Channels", "Status"];
  const rows = state.devices.map((d) => [d.name, "IP/Domain", `${d.host}:${d.port}`, d.type, d.vendor || "", d.channels, d.status]);
  const csv = [header, ...rows].map((row) => row.map((cell) => `"${String(cell).replace(/"/g, '""')}"`).join(",")).join("\n");
  const blob = new Blob([csv], { type: "text/csv" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = "devices.csv";
  link.click();
  URL.revokeObjectURL(url);
});

els.deviceFilterInput?.addEventListener("input", () => renderDevices());

els.deviceSelectAll?.addEventListener("change", () => {
  const query = (els.deviceFilterInput?.value || "").trim().toLowerCase();
  const filtered = state.devices.filter((device) =>
    !query || [device.name, device.type, device.vendor, device.host].join(" ").toLowerCase().includes(query)
  );
  if (els.deviceSelectAll.checked) {
    filtered.forEach((d) => checkedDeviceIds.add(d.id));
  } else {
    filtered.forEach((d) => checkedDeviceIds.delete(d.id));
  }
  renderDevices();
});
els.resetDeviceFormBtn.addEventListener("click", resetDeviceForm);
els.cameraForm.addEventListener("submit", saveCamera);
els.resetCameraFormBtn.addEventListener("click", resetCameraForm);
els.cameraDeviceInput.addEventListener("change", () => renderCameraChannelOptions(els.cameraDeviceInput.value, 1));
els.cameraChannelSelect.addEventListener("change", () => {
  els.cameraChannelInput.value = els.cameraChannelSelect.value;
});
els.mapAreaInput.addEventListener("change", (event) => {
  state.mapArea = event.target.value;
  persist();
  renderMap();
});
els.mapCameraInput.addEventListener("change", (event) => setMapSelected(event.target.value));
els.saveMapPositionBtn.addEventListener("click", saveMapPosition);
els.autoPlaceMapBtn.addEventListener("click", autoPlaceMap);
// --- Remote Playback: tree search, grid toolbar, range popover, scrubber --

els.pbResourceTabs.forEach((tab) => {
  tab.addEventListener("click", () => {
    els.pbResourceTabs.forEach((item) => item.classList.toggle("active", item === tab));
    const showResource = tab.dataset.pbResourceTab === "resource";
    if (els.pbResourcePane) els.pbResourcePane.hidden = !showResource;
    if (els.pbAutoSwitchPane) els.pbAutoSwitchPane.hidden = showResource;
  });
});

els.pbTreeSearchInput?.addEventListener("input", (event) => {
  state.playbackQuery = event.target.value;
  renderPlaybackTree();
});

function clearPlaybackGrid() {
  maximizedPlaybackCameraId = "";
  state.playbackOpenIds = [];
  state.playbackSelectedId = "";
  persist();
  refreshWorkspaceResourceUi();
  renderPlaybackTileGrid();
  renderPlaybackTree();
}

function closeAllPlaybackPopovers() {
  if (els.pbLayoutPopover) els.pbLayoutPopover.hidden = true;
  if (els.pbRangePopover) els.pbRangePopover.hidden = true;
}

els.pbLayoutBtn?.addEventListener("click", (event) => {
  event.stopPropagation();
  const willShow = els.pbLayoutPopover.hidden;
  closeAllPlaybackPopovers();
  els.pbLayoutPopover.hidden = !willShow;
  if (willShow && els.pbDivisionCustomInput) els.pbDivisionCustomInput.value = state.playbackGridDivision;
});

els.pbLayoutPopover?.querySelectorAll("[data-pb-division]").forEach((button) => {
  button.addEventListener("click", () => {
    setPlaybackDivision(Number(button.dataset.pbDivision));
    closeAllPlaybackPopovers();
  });
});

els.pbDivisionCustomBtn?.addEventListener("click", (event) => {
  event.stopPropagation();
  if (!els.pbDivisionCustomInput?.value) return;
  setPlaybackDivision(els.pbDivisionCustomInput.value);
  closeAllPlaybackPopovers();
});
els.pbLayoutPopover?.addEventListener("click", (event) => event.stopPropagation());

els.pbCloseAllBtn?.addEventListener("click", clearPlaybackGrid);

els.pbFullscreenBtn?.addEventListener("click", () => {
  if (document.fullscreenElement) {
    document.exitFullscreen();
  } else if (els.playbackPanel?.requestFullscreen) {
    els.playbackPanel.requestFullscreen().catch(() => {});
  }
});

els.pbRangeBtn?.addEventListener("click", (event) => {
  event.stopPropagation();
  const willShow = els.pbRangePopover.hidden;
  closeAllPlaybackPopovers();
  if (willShow) openPlaybackRangePopover();
});

els.pbRangeApplyBtn?.addEventListener("click", (event) => {
  event.stopPropagation();
  applyPlaybackRange();
});

els.pbRangePopover?.addEventListener("click", (event) => event.stopPropagation());
els.pbLayoutPopover?.addEventListener("click", (event) => event.stopPropagation());

document.addEventListener("click", () => closeAllPlaybackPopovers());

els.pbJumpStartBtn?.addEventListener("click", () => {
  stopPlaybackPlay();
  playbackCursorDate = startOfDay(playbackActiveDay);
  renderPlaybackScrubber();
});
els.pbJumpEndBtn?.addEventListener("click", () => {
  stopPlaybackPlay();
  playbackCursorDate = new Date(startOfDay(playbackActiveDay).getTime() + 86400000 - 1000);
  renderPlaybackScrubber();
});
els.pbStepBackBtn?.addEventListener("click", () => {
  stopPlaybackPlay();
  stepPlayback(-10);
});
els.pbStepForwardBtn?.addEventListener("click", () => {
  stopPlaybackPlay();
  stepPlayback(10);
});
els.pbPlayBtn?.addEventListener("click", togglePlaybackPlay);
els.pbSpeedBtn?.addEventListener("click", () => {
  playbackSpeedIndex = (playbackSpeedIndex + 1) % playbackSpeeds.length;
  renderPlaybackScrubber();
});
els.pbPrevDayBtn?.addEventListener("click", () => changePlaybackDay(-1));
els.pbNextDayBtn?.addEventListener("click", () => changePlaybackDay(1));

els.pbTrack?.addEventListener("pointerdown", (event) => {
  playbackScrubbing = true;
  stopPlaybackPlay();
  seekPlaybackToClientX(event.clientX);
  els.pbTrack.setPointerCapture(event.pointerId);
});
els.pbTrack?.addEventListener("pointermove", (event) => {
  if (!playbackScrubbing) return;
  seekPlaybackToClientX(event.clientX);
});
els.pbTrack?.addEventListener("pointerup", (event) => {
  playbackScrubbing = false;
  els.pbTrack.releasePointerCapture(event.pointerId);
});
els.incidentStatusFilter.addEventListener("change", (event) => {
  state.incidentStatus = event.target.value;
  renderIncidents();
});
els.incidentSearchInput.addEventListener("input", (event) => {
  state.incidentQuery = event.target.value;
  renderIncidents();
});
els.groupForm.addEventListener("submit", saveGroup);
els.resetGroupFormBtn.addEventListener("click", resetGroupForm);
els.groupDeviceFilterInput.addEventListener("change", renderGroupCameraOptions);
els.importSelectedCamerasBtn.addEventListener("click", importSelectedCamerasIntoGroup);
els.importAllDeviceCamerasBtn.addEventListener("click", importAllDeviceChannelsIntoGroup);
els.userForm.addEventListener("submit", saveUser);
els.resetUserFormBtn.addEventListener("click", resetUserForm);

window.history.replaceState({ view: state.view }, "", `#${state.view}`);
window.addEventListener("popstate", (event) => {
  const hashView = window.location.hash ? window.location.hash.slice(1) : "";
  const view = event.state?.view || hashView || "operator";
  setView(validViews.has(view) ? view : "operator");
});

buildPlaybackTicks();
enforceGlobalCameraLimit();
render();
if (pendingCameraLinkMigration) {
  pendingCameraLinkMigration = false;
  persist();
}
void initializeInventoryBackend();
}
