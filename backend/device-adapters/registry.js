const {
  DeviceAdapterContractError,
  DeviceAdapterOperationError,
  createDeviceAdapter,
  normalizeAdapterId,
  normalizeOperationId
} = require("./contract");

class DeviceAdapterRegistry {
  constructor() {
    this.adapters = new Map();
    this.aliases = new Map();
  }

  register(adapterDefinition) {
    const adapter = adapterDefinition?.manifest?.contractVersion
      ? createDeviceAdapter(adapterDefinition)
      : createDeviceAdapter(adapterDefinition || {});
    const { id, aliases } = adapter.manifest;

    if (this.adapters.has(id)) {
      throw new DeviceAdapterContractError("DUPLICATE_ADAPTER_ID", `Adapter id is already registered: ${id}`);
    }

    aliases.forEach((alias) => {
      const owner = this.aliases.get(alias);
      if (owner) {
        throw new DeviceAdapterContractError(
          "DUPLICATE_ADAPTER_ALIAS",
          `Adapter alias ${alias} is already owned by ${owner}.`
        );
      }
    });

    this.adapters.set(id, adapter);
    aliases.forEach((alias) => this.aliases.set(alias, id));
    return this;
  }

  resolve(adapterId) {
    const requestedId = normalizeAdapterId(adapterId);
    if (!requestedId) {
      return {
        registered: false,
        requestedId: "",
        adapter: null,
        manifest: null,
        reasonCode: "ADAPTER_ID_REQUIRED"
      };
    }

    const canonicalId = this.aliases.get(requestedId) || requestedId;
    const adapter = this.adapters.get(canonicalId) || null;
    return {
      registered: Boolean(adapter),
      requestedId,
      adapter,
      manifest: adapter?.manifest || null,
      reasonCode: adapter ? "" : "ADAPTER_NOT_REGISTERED"
    };
  }

  list() {
    return [...this.adapters.values()].map((adapter) => adapter.manifest);
  }

  async execute(adapterId, operationId, context = {}) {
    const operation = normalizeOperationId(operationId);
    const resolution = this.resolve(adapterId);
    if (!resolution.registered) {
      return {
        status: "unavailable",
        adapterId: resolution.requestedId,
        operation,
        reasonCode: resolution.reasonCode,
        message: resolution.reasonCode === "ADAPTER_ID_REQUIRED"
          ? "An explicit adapter id is required."
          : `No adapter is registered for ${resolution.requestedId}.`
      };
    }

    const handler = resolution.adapter.operations[operation];
    if (!handler) {
      if (resolution.adapter.unsupportedOperations.includes(operation)) {
        return {
          status: "unsupported",
          adapterId: resolution.manifest.id,
          operation,
          reasonCode: "OPERATION_UNSUPPORTED",
          message: `${resolution.manifest.displayName} declares operation ${operation} unsupported.`
        };
      }
      return {
        status: "unknown",
        adapterId: resolution.manifest.id,
        operation,
        reasonCode: "OPERATION_NOT_VERIFIED",
        message: `${resolution.manifest.displayName} operation ${operation} has not been verified or implemented.`
      };
    }

    try {
      const data = await handler(context);
      return {
        status: "succeeded",
        adapterId: resolution.manifest.id,
        operation,
        reasonCode: "",
        message: "",
        data
      };
    } catch (error) {
      const outcomeUnknown =
        error instanceof DeviceAdapterOperationError &&
        error.outcomeUnknown === true;
      return {
        status: outcomeUnknown ? "unknown" : "failed",
        adapterId: resolution.manifest.id,
        operation,
        reasonCode: error.code || (outcomeUnknown ? "OPERATION_OUTCOME_UNKNOWN" : "ADAPTER_OPERATION_FAILED"),
        message: error.message || (outcomeUnknown
          ? "The operation may have executed, but its result could not be confirmed."
          : "Adapter operation failed."),
        ...(outcomeUnknown ? { outcomeUnknown: true } : {})
      };
    }
  }
}

module.exports = {
  DeviceAdapterRegistry
};
