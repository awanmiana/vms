const contract = require("./contract");
const { createReferenceVendorAdapter } = require("./reference");
const { DeviceAdapterRegistry } = require("./registry");

function createDefaultDeviceAdapterRegistry() {
  return new DeviceAdapterRegistry().register(createReferenceVendorAdapter());
}

module.exports = {
  ...contract,
  DeviceAdapterRegistry,
  createDefaultDeviceAdapterRegistry,
  createReferenceVendorAdapter
};
