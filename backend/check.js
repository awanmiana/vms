const { getSchemaStatements } = require("./schema-loader");
const { canOpenTier, estimateBitrateKbps, resolveTier } = require("./media-policy");
const { FileDatabase } = require("./file-db");
const { CameraService, DeviceService, MediaGuardService } = require("./services");
const { CommandExecutor } = require("./commands");
const { RegexCommandParser, createAliasResolver } = require("./voice-regex");

const statements = getSchemaStatements();
const db = new FileDatabase();
db.load();

const devices = new DeviceService(db);
const cameras = new CameraService(db);
const guard = new MediaGuardService(db);
const commands = new CommandExecutor(db, { mediaGuard: guard });

const device = devices.save({
  id: "dev-check",
  name: "Check NVR",
  type: "NVR",
  host: "192.168.10.10",
  port: 8000,
  channelCount: 4,
  maxConcurrentMainstream: 1,
  maxConcurrentSubstream: 8
});
const syncedCameras = cameras.syncDeviceChannels(device);
const tier = resolveTier({
  tileCount: 16,
  isFocused: false,
  isTracking: false,
  paneContext: "grid",
  isVisible: true
});

const allowed = canOpenTier({
  tier: "main",
  deviceUsage: { main: 3, sub: 12 },
  deviceLimits: { maxConcurrentMainstream: 4, maxConcurrentSubstream: 32 }
});
const openPlan = guard.planOpen({
  camera: syncedCameras[0],
  device,
  paneContext: "grid",
  tileCount: 4
});

db.upsert("voiceAliases", {
  id: "alias-check-front-door",
  entityType: "camera",
  entityId: syncedCameras[0].id,
  alias: "front door",
  normalizedAlias: "front door"
});
const parser = new RegexCommandParser(createAliasResolver(db));
const parsedCommands = parser.parse("please open front door and start tracking front door");
const commandResults = parsedCommands.map((command) => commands.execute(command));

console.log(JSON.stringify({
  schemaStatements: statements.length,
  deviceCount: devices.list().length,
  syncedChannelCount: syncedCameras.length,
  sampleTier: tier,
  sampleBitrateKbps: estimateBitrateKbps(tier),
  sampleMainAllowed: allowed,
  sampleOpenPlan: openPlan,
  parsedCommandCount: parsedCommands.length,
  commandResultStatuses: commandResults.map((result) => result.status),
  breadcrumbCount: db.table("sessionBreadcrumbs").length
}, null, 2));
