const { FileDatabase } = require("./file-db");
const { CameraService, DeviceOnboardingService, DeviceService, MediaGuardService } = require("./services");
const {
  ComplianceLogService,
  ComplianceTypeService,
  EntityLocationService,
  TagIndexService,
  TicketService,
  seedComplianceTypes
} = require("./compliance-services");
const { handleStubRoute } = require("./api-routes");

function createServices() {
  const db = new FileDatabase();
  db.load();
  const tags = new TagIndexService(db);
  const complianceTypes = new ComplianceTypeService(db, tags);
  return {
    cameras: new CameraService(db),
    complianceLogs: new ComplianceLogService(db),
    complianceTypes,
    db,
    devices: new DeviceService(db),
    entities: new EntityLocationService(db, tags),
    guard: new MediaGuardService(db),
    onboarding: new DeviceOnboardingService(db),
    tags,
    tickets: new TicketService(db)
  };
}

function print(value) {
  console.log(JSON.stringify(value, null, 2));
}

function main() {
  const [command, ...args] = process.argv.slice(2);
  const { cameras, complianceTypes, devices, entities, guard, onboarding, tags, tickets } = createServices();

  if (command === "devices:list") {
    print(devices.list());
    return;
  }

  if (command === "cameras:list") {
    print(cameras.list());
    return;
  }

  if (command === "compliance-types:seed") {
    print(seedComplianceTypes(complianceTypes));
    return;
  }

  if (command === "entities:add") {
    print(entities.createEntity({ id: args[0], name: args[1] || "Super Space Ocean Mall", alias: args[2] || "SSOM" }));
    return;
  }

  if (command === "locations:add") {
    print(entities.createLocation({ id: args[0], entityId: args[1], name: args[2] || "VR Station" }));
    return;
  }

  if (command === "tags:search") {
    print(tags.search(args.join(" ")));
    return;
  }

  if (command === "tickets:list") {
    print(tickets.listTickets(args[0] || ""));
    return;
  }

  if (command === "api:stub") {
    print(handleStubRoute(args[0] || "GET", args[1] || "/api/entities"));
    return;
  }

  if (command === "devices:add-demo") {
    const result = onboarding.onboard({
      id: args[0] || "dev-demo",
      name: args[1] || "Demo NVR",
      type: "NVR",
      host: args[2] || "192.168.1.50",
      port: 8000,
      channelCount: Number(args[3] || 8),
      maxConcurrentMainstream: 2,
      maxConcurrentSubstream: 16
    });
    print(result);
    return;
  }

  if (command === "guard:plan") {
    const camera = cameras.list()[0];
    const device = devices.list().find((item) => item.id === camera?.deviceId);
    if (!camera || !device) {
      print({ error: "Add or sync a device first." });
      return;
    }
    print(guard.planOpen({ camera, device, paneContext: "grid", tileCount: Number(args[0] || 4) }));
    return;
  }

  print({
    commands: [
      "devices:list",
      "cameras:list",
      "devices:add-demo [id] [name] [host] [channels]",
      "guard:plan [tileCount]",
      "compliance-types:seed",
      "entities:add [id] [name] [alias]",
      "locations:add [id] [entityId] [name]",
      "tags:search [query]",
      "tickets:list [status]",
      "api:stub [method] [path]"
    ]
  });
}

main();
