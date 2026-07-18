const assert = require("assert");
const path = require("path");
const { FileDatabase } = require("./file-db");
const {
  ComplianceLogService,
  ComplianceTypeService,
  EntityLocationService,
  TagIndexService,
  TicketService,
  seedComplianceTypes
} = require("./compliance-services");
const { handleStubRoute } = require("./api-routes");
const { formatDateTime } = require("./datetime");

function freshDb() {
  const db = new FileDatabase(path.join(__dirname, "test-compliance-unused.json"));
  db.memoryOnly = true;
  db.load();
  return db;
}

function run(name, fn) {
  try {
    fn();
    console.log(`ok - ${name}`);
  } catch (error) {
    console.error(`FAIL - ${name}`);
    console.error(error);
    process.exitCode = 1;
  }
}

run("formatDateTime uses canonical VMS timestamp format", () => {
  const value = formatDateTime(new Date(2026, 6, 14, 23, 1, 59));
  assert.strictEqual(value, "2026-07-14 23:01:59");
});

run("creating entities and locations also writes tag index rows", () => {
  const db = freshDb();
  const tags = new TagIndexService(db);
  const entities = new EntityLocationService(db, tags);

  const entity = entities.createEntity({ id: "ent-ssom", name: "Super Space Ocean Mall", alias: "SSOM" });
  const location = entities.createLocation({ id: "loc-vr", entityId: entity.id, name: "VR Station" });

  assert.strictEqual(location.entityId, "ent-ssom");
  assert.ok(tags.search("ssom").some((tag) => tag.referenceId === "ent-ssom"));
  assert.ok(tags.search("vr").some((tag) => tag.referenceId === "loc-vr"));
});

run("seeded compliance types are searchable tags", () => {
  const db = freshDb();
  const tags = new TagIndexService(db);
  const complianceTypes = new ComplianceTypeService(db, tags);

  seedComplianceTypes(complianceTypes);

  assert.ok(complianceTypes.list().some((type) => type.label === "Sale not punched"));
  assert.ok(tags.search("sale").some((tag) => tag.tagType === "compliance_type"));
});

run("compliance logs capture camera and location snapshots", () => {
  const db = freshDb();
  const tags = new TagIndexService(db);
  const entities = new EntityLocationService(db, tags);
  const complianceTypes = new ComplianceTypeService(db, tags);
  const logs = new ComplianceLogService(db);

  db.upsert("cameras", {
    id: "cam-vr-1",
    displayName: "VR Station Entry",
    deviceId: "dev-1",
    channelNumber: 4
  });
  entities.createEntity({ id: "ent-ssom", name: "Super Space Ocean Mall", alias: "SSOM" });
  entities.createLocation({ id: "loc-vr", entityId: "ent-ssom", name: "VR Station" });
  complianceTypes.save({ id: "ctype-sale", label: "Sale not punched", category: "non_compliance" });

  const log = logs.createLog({
    entityId: "ent-ssom",
    complianceTypeId: "ctype-sale",
    reviewStartAt: "2026-07-14 09:00:00",
    reviewEndAt: "2026-07-14 09:15:00",
    sourceWhatsappMessageId: "wa-op-1",
    managerResponseWhatsappMessageId: "wa-manager-1",
    cameraIds: ["cam-vr-1"],
    locationIds: ["loc-vr"]
  });

  assert.strictEqual(log.entityNameSnapshot, "Super Space Ocean Mall");
  assert.strictEqual(db.table("complianceLogCameras")[0].cameraDisplayNameSnapshot, "VR Station Entry");
  assert.strictEqual(db.table("complianceLogLocations")[0].locationNameSnapshot, "VR Station");
});

run("tickets support internal CRUD without external parsing", () => {
  const db = freshDb();
  const tickets = new TicketService(db);
  const ticket = tickets.createTicket({
    queryType: "other",
    rawMessageText: "QUERY Entity: SSOM",
    requestedStartAt: "2026-07-14 09:00:00",
    requestedEndAt: "2026-07-14 09:15:00"
  });

  assert.strictEqual(tickets.listTickets("pending").length, 1);
  assert.strictEqual(tickets.updateTicketStatus(ticket.id, "resolved").status, "resolved");
});

run("future external integration routes are explicit 501 stubs", () => {
  const response = handleStubRoute("POST", "/api/compliance-logs");
  assert.strictEqual(response.status, 501);
  assert.match(response.body.message, /future external tools/);
});

if (process.exitCode) {
  console.error("\nSome tests failed.");
} else {
  console.log("\nAll compliance service tests passed.");
}
