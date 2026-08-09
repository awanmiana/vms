const { FileDatabase } = require("./file-db");
const { createRepositories } = require("./repositories");
const { ResiliencePolicyService } = require("./resilience");
const { CameraService, DeviceOnboardingService, DeviceService, MediaGuardService } = require("./services");
const { CommandExecutor } = require("./commands");
const { DeviceIntegrationService } = require("./device-integration");
const { createDefaultDeviceAdapterRegistry } = require("./device-adapters");
const {
  ComplianceLogService,
  ComplianceTypeService,
  EntityLocationService,
  TagIndexService,
  TicketService
} = require("./compliance-services");

function createBackendComposition({ db, databasePath, adapterRegistry, resiliencePolicy } = {}) {
  const database = db || new FileDatabase(databasePath || undefined);
  if (!db) database.load();
  const repositories = createRepositories(database);
  const resilience = resiliencePolicy || new ResiliencePolicyService();
  const devices = new DeviceService(repositories);
  const cameras = new CameraService(repositories);
  const guard = new MediaGuardService(repositories);
  const onboarding = new DeviceOnboardingService({ devices, cameras });
  const tags = new TagIndexService(repositories.store);
  const complianceTypes = new ComplianceTypeService(repositories.store, tags);
  const services = Object.freeze({
    cameras,
    commands: new CommandExecutor(repositories.store, { mediaGuard: guard, resiliencePolicy: resilience }),
    complianceLogs: new ComplianceLogService(repositories.store),
    complianceTypes,
    deviceIntegration: new DeviceIntegrationService(
      repositories.store,
      adapterRegistry || createDefaultDeviceAdapterRegistry(),
      { resiliencePolicy: resilience }
    ),
    devices,
    entities: new EntityLocationService(repositories.store, tags),
    guard,
    onboarding,
    tags,
    tickets: new TicketService(repositories.store)
  });
  return Object.freeze({ database, repositories, resiliencePolicy: resilience, services });
}

module.exports = { createBackendComposition };
