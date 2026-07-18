const ADAPTER_CONTRACT_VERSION = 1;

const OPERATION_STATUSES = Object.freeze([
  "succeeded",
  "failed",
  "unsupported",
  "unknown",
  "unavailable"
]);

class DeviceAdapterContractError extends Error {
  constructor(code, message) {
    super(message);
    this.name = "DeviceAdapterContractError";
    this.code = code;
  }
}

class DeviceAdapterOperationError extends Error {
  constructor(code, message, { outcomeUnknown = false } = {}) {
    super(message);
    this.name = "DeviceAdapterOperationError";
    this.code = String(code || "ADAPTER_OPERATION_FAILED");
    this.outcomeUnknown = outcomeUnknown === true;
  }
}

function normalizeAdapterId(value) {
  return String(value || "")
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "");
}

function normalizeOperationId(value) {
  const operationId = String(value || "").trim().toLowerCase();
  if (!/^[a-z][a-z0-9-]*(?:\.[a-z][a-z0-9-]*)+$/.test(operationId)) {
    throw new DeviceAdapterContractError(
      "INVALID_ADAPTER_OPERATION",
      `Invalid adapter operation id: ${value || "(empty)"}`
    );
  }
  return operationId;
}

function normalizeManifest(manifest) {
  if (!manifest || typeof manifest !== "object" || Array.isArray(manifest)) {
    throw new DeviceAdapterContractError("INVALID_ADAPTER_MANIFEST", "Adapter manifest must be an object.");
  }

  if (manifest.contractVersion !== ADAPTER_CONTRACT_VERSION) {
    throw new DeviceAdapterContractError(
      "UNSUPPORTED_ADAPTER_CONTRACT",
      `Adapter contract version must be ${ADAPTER_CONTRACT_VERSION}.`
    );
  }

  const id = normalizeAdapterId(manifest.id);
  if (!id) {
    throw new DeviceAdapterContractError("INVALID_ADAPTER_ID", "Adapter manifest id is required.");
  }

  const displayName = String(manifest.displayName || "").trim();
  if (!displayName) {
    throw new DeviceAdapterContractError("INVALID_ADAPTER_NAME", "Adapter displayName is required.");
  }

  const aliases = [...new Set([id, ...(Array.isArray(manifest.aliases) ? manifest.aliases : [])]
    .map(normalizeAdapterId)
    .filter(Boolean))];

  return Object.freeze({
    contractVersion: ADAPTER_CONTRACT_VERSION,
    id,
    displayName,
    aliases: Object.freeze(aliases),
    implementationStatus: String(manifest.implementationStatus || "identity-only").trim() || "identity-only"
  });
}

function createDeviceAdapter({ manifest, operations = {}, unsupportedOperations = [] } = {}) {
  const normalizedManifest = normalizeManifest(manifest);
  if (!operations || typeof operations !== "object" || Array.isArray(operations)) {
    throw new DeviceAdapterContractError("INVALID_ADAPTER_OPERATIONS", "Adapter operations must be an object.");
  }

  const normalizedOperations = {};
  Object.entries(operations).forEach(([operationId, handler]) => {
    const normalizedId = normalizeOperationId(operationId);
    if (typeof handler !== "function") {
      throw new DeviceAdapterContractError(
        "INVALID_ADAPTER_HANDLER",
        `Adapter operation ${normalizedId} must be a function.`
      );
    }
    normalizedOperations[normalizedId] = handler;
  });

  if (!Array.isArray(unsupportedOperations)) {
    throw new DeviceAdapterContractError(
      "INVALID_UNSUPPORTED_OPERATIONS",
      "unsupportedOperations must be an array."
    );
  }
  const normalizedUnsupportedOperations = [...new Set(unsupportedOperations.map(normalizeOperationId))];
  normalizedUnsupportedOperations.forEach((operationId) => {
    if (normalizedOperations[operationId]) {
      throw new DeviceAdapterContractError(
        "CONFLICTING_ADAPTER_OPERATION",
        `Adapter operation ${operationId} cannot be both implemented and unsupported.`
      );
    }
  });

  return Object.freeze({
    manifest: normalizedManifest,
    operations: Object.freeze(normalizedOperations),
    unsupportedOperations: Object.freeze(normalizedUnsupportedOperations)
  });
}

module.exports = {
  ADAPTER_CONTRACT_VERSION,
  DeviceAdapterContractError,
  DeviceAdapterOperationError,
  OPERATION_STATUSES,
  createDeviceAdapter,
  normalizeAdapterId,
  normalizeManifest,
  normalizeOperationId
};
