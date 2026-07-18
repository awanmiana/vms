const crypto = require("crypto");
const { formatDateTime } = require("./datetime");
const { normalizeLookup } = require("./commands");

function id(prefix) {
  return `${prefix}-${crypto.randomUUID()}`;
}

function normalizeTag(value) {
  return normalizeLookup(value);
}

class TagIndexService {
  constructor(db) {
    this.db = db;
  }

  upsertTag({ tagType, referenceId = "", label }) {
    const normalizedLabel = normalizeTag(label);
    const existing = this.db
      .table("tagIndex")
      .find((item) => item.tagType === tagType && item.normalizedLabel === normalizedLabel && String(item.referenceId || "") === String(referenceId || ""));

    const row = {
      id: existing?.id || id("tag"),
      tagType,
      referenceId,
      label,
      normalizedLabel
    };
    this.db.upsert("tagIndex", row);
    return row;
  }

  search(query) {
    const wanted = normalizeTag(query);
    if (!wanted) return [];
    return this.db.table("tagIndex").filter((tag) => tag.normalizedLabel.includes(wanted));
  }
}

class EntityLocationService {
  constructor(db, tagIndex = new TagIndexService(db)) {
    this.db = db;
    this.tagIndex = tagIndex;
  }

  createEntity({ id: entityId, name, alias = "", notes = "" }) {
    const entity = {
      id: entityId || id("ent"),
      name,
      alias,
      notes
    };
    this.db.upsert("entities", entity);
    this.tagIndex.upsertTag({ tagType: "entity", referenceId: entity.id, label: name });
    if (alias) {
      this.tagIndex.upsertTag({ tagType: "entity", referenceId: entity.id, label: alias });
    }
    return entity;
  }

  createLocation({ id: locationId, entityId, name, locationType = "", notes = "" }) {
    const entity = this.db.table("entities").find((item) => item.id === entityId);
    if (!entity) throw new Error(`Entity not found: ${entityId}`);

    const location = {
      id: locationId || id("loc"),
      entityId,
      name,
      locationType,
      notes
    };
    this.db.upsert("locations", location);
    this.tagIndex.upsertTag({ tagType: "location", referenceId: location.id, label: name });
    return location;
  }

  assignCameraToLocation({ cameraId, locationId, coverageRole = "primary", notes = "" }) {
    const location = this.db.table("locations").find((item) => item.id === locationId);
    if (!location) throw new Error(`Location not found: ${locationId}`);

    const row = {
      id: `${cameraId}:${locationId}`,
      cameraId,
      locationId,
      coverageRole,
      notes
    };
    this.db.upsert("cameraLocations", row);
    return row;
  }

  listEntities() {
    return this.db.table("entities");
  }

  listLocations(entityId = "") {
    return this.db.table("locations").filter((location) => !entityId || location.entityId === entityId);
  }
}

class ComplianceTypeService {
  constructor(db, tagIndex = new TagIndexService(db)) {
    this.db = db;
    this.tagIndex = tagIndex;
  }

  save({ id: typeId, label, category = "non_compliance", createdVia = "preset", reviewStatus = "approved" }) {
    const row = {
      id: typeId || id("ctype"),
      label,
      normalizedLabel: normalizeTag(label),
      category,
      createdVia,
      reviewStatus
    };
    this.db.upsert("complianceTypes", row);
    this.tagIndex.upsertTag({ tagType: "compliance_type", referenceId: row.id, label });
    return row;
  }

  list() {
    return this.db.table("complianceTypes");
  }
}

class ComplianceLogService {
  constructor(db) {
    this.db = db;
  }

  createLog({ entityId, complianceTypeId, reviewStartAt, reviewEndAt, loggedByUserId = "", sourceWhatsappMessageId = "", managerResponseWhatsappMessageId = "", managerResponseText = "", notes = "", cameraIds = [], locationIds = [] }) {
    const entity = this.db.table("entities").find((item) => item.id === entityId);
    const type = this.db.table("complianceTypes").find((item) => item.id === complianceTypeId);
    if (!entity) throw new Error(`Entity not found: ${entityId}`);
    if (!type) throw new Error(`Compliance type not found: ${complianceTypeId}`);

    const log = {
      id: id("clog"),
      entityId,
      entityNameSnapshot: entity.name,
      complianceTypeId,
      complianceTypeLabelSnapshot: type.label,
      reviewStartAt,
      reviewEndAt,
      loggedByUserId,
      sourceWhatsappMessageId,
      managerResponseWhatsappMessageId,
      managerResponseText,
      notes
    };
    this.db.upsert("complianceLogs", log);

    cameraIds.forEach((cameraId) => this.addCameraSnapshot(log.id, cameraId));
    locationIds.forEach((locationId) => this.addLocationSnapshot(log.id, locationId));
    return log;
  }

  addCameraSnapshot(complianceLogId, cameraId) {
    const camera = this.db.table("cameras").find((item) => item.id === cameraId);
    const row = {
      id: `${complianceLogId}:${cameraId}`,
      complianceLogId,
      cameraId,
      cameraDisplayNameSnapshot: camera?.displayName || camera?.name || cameraId,
      deviceNameSnapshot: camera?.nvr || camera?.deviceId || "",
      channelNumberSnapshot: Number(camera?.channelNumber || camera?.channel || 0)
    };
    this.db.upsert("complianceLogCameras", row);
    return row;
  }

  addLocationSnapshot(complianceLogId, locationId) {
    const location = this.db.table("locations").find((item) => item.id === locationId);
    const row = {
      id: `${complianceLogId}:${locationId}`,
      complianceLogId,
      locationId,
      locationNameSnapshot: location?.name || locationId
    };
    this.db.upsert("complianceLogLocations", row);
    return row;
  }

  listLogs() {
    return this.db.table("complianceLogs");
  }
}

class TicketService {
  constructor(db) {
    this.db = db;
  }

  createTicket({ entityId = "", locationId = "", queryType = "other", rawMessageText = "", requestedStartAt = "", requestedEndAt = "", subjectDescriptor = "", whatsappMessageId = "" }) {
    const ticket = {
      id: id("ticket"),
      entityId,
      locationId,
      queryType,
      status: "pending",
      rawMessageText,
      requestedStartAt,
      requestedEndAt,
      subjectDescriptor,
      linkedTrackingSessionId: "",
      whatsappMessageId,
      createdAt: formatDateTime(),
      resolvedAt: ""
    };
    this.db.upsert("tickets", ticket);
    return ticket;
  }

  listTickets(status = "") {
    return this.db.table("tickets").filter((ticket) => !status || ticket.status === status);
  }

  updateTicketStatus(id, status) {
    const ticket = this.db.table("tickets").find((item) => item.id === id);
    if (!ticket) throw new Error(`Ticket not found: ${id}`);
    const next = {
      ...ticket,
      status,
      resolvedAt: status === "resolved" ? formatDateTime() : ticket.resolvedAt || ""
    };
    this.db.upsert("tickets", next);
    return next;
  }
}

function seedComplianceTypes(complianceTypes) {
  const presets = [
    { id: "ctype-no-cashier", label: "No cashier at counter", category: "non_compliance" },
    { id: "ctype-staff-no-uniform", label: "Staff not in uniform", category: "non_compliance" },
    { id: "ctype-sale-not-punched", label: "Sale not punched", category: "non_compliance" }
  ];
  presets.forEach((preset) => {
    if (!complianceTypes.list().some((item) => item.id === preset.id)) {
      complianceTypes.save(preset);
    }
  });
  return complianceTypes.list();
}

module.exports = {
  ComplianceLogService,
  ComplianceTypeService,
  EntityLocationService,
  TagIndexService,
  TicketService,
  seedComplianceTypes
};
