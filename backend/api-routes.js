const { formatDateTime } = require("./datetime");
const {
  INVENTORY_VERSION,
  InventoryValidationError,
  SUPPORTED_DEVICE_TYPES,
  normalizeInventory
} = require("../shared/inventory/contract");
const {
  httpStatusForOutcome,
  persistenceOutcome
} = require("../shared/resilience/outcome-mapping");

const DEVICE_TYPES = new Set(SUPPORTED_DEVICE_TYPES);

function notImplemented(route) {
  return {
    status: 501,
    body: {
      error: "Not Implemented",
      route,
      message: "Reserved VMS integration endpoint for future external tools."
    }
  };
}

const routes = {
  "POST /api/compliance-logs": () => notImplemented("POST /api/compliance-logs"),
  "POST /api/tickets": () => notImplemented("POST /api/tickets"),
  "GET /api/entities": () => notImplemented("GET /api/entities"),
  "GET /api/locations": () => notImplemented("GET /api/locations"),
  "GET /api/compliance-types": () => notImplemented("GET /api/compliance-types")
};

function handleStubRoute(method, pathname) {
  const key = `${String(method || "").toUpperCase()} ${pathname}`;
  const handler = routes[key];
  if (!handler) {
    return {
      status: 404,
      body: {
        error: "Not Found",
        route: key
      }
    };
  }
  return handler();
}

function isInventoryInitialized(repository) {
  return repository.isInitialized();
}

function emptyInventorySnapshot() {
  return { initialized: false, devices: [], cameras: [], groups: [] };
}

function readInventory(repository) {
  if (!isInventoryInitialized(repository)) return emptyInventorySnapshot();
  const inventory = normalizeInventory(repository.readTables());
  return { initialized: true, ...inventory };
}

function persistInventory(repository, payload) {
  const inventory = normalizeInventory(payload);
  repository.replace({
    devices: inventory.devices,
    cameras: inventory.cameras,
    groups: inventory.groups,
    inventoryState: {
      initialized: true,
      version: INVENTORY_VERSION,
      updatedAt: formatDateTime()
    }
  });

  return { initialized: true, ...inventory };
}

function handleInventoryRoute(repository, method, pathname, body) {
  if (pathname !== "/api/inventory") return null;
  const normalizedMethod = String(method || "").toUpperCase();

  try {
    if (normalizedMethod === "GET") {
      return { status: 200, body: readInventory(repository) };
    }
    if (normalizedMethod === "PUT") {
      return { status: 200, body: persistInventory(repository, body) };
    }
    return {
      status: 405,
      headers: { Allow: "GET, PUT" },
      body: { error: "Method Not Allowed", route: `${normalizedMethod} ${pathname}` }
    };
  } catch (error) {
    if (error instanceof InventoryValidationError) {
      return {
        status: 400,
        body: {
          error: "Invalid inventory",
          message: error.message,
          outcome: "failed",
          reasonCode: "INVENTORY_VALIDATION_FAILED"
        }
      };
    }
    if (error.code === "PERSISTENCE_UNAVAILABLE") {
      const outcome = persistenceOutcome(error);
      return {
        status: httpStatusForOutcome(outcome),
        body: {
          error: "Inventory persistence unavailable",
          message: error.message,
          durable: false,
          outcome,
          reasonCode: error.code
        }
      };
    }
    const outcome = persistenceOutcome(error);
    return {
      status: httpStatusForOutcome(outcome),
      body: {
        error: "Inventory persistence failed",
        message: error.message,
        outcome,
        reasonCode: error.code || "PERSISTENCE_FAILED"
      }
    };
  }
}

module.exports = {
  DEVICE_TYPES,
  INVENTORY_VERSION,
  InventoryValidationError,
  handleInventoryRoute,
  handleStubRoute,
  isInventoryInitialized,
  normalizeInventory,
  persistInventory,
  readInventory,
  routes
};
