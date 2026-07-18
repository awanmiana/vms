const { ADAPTER_CONTRACT_VERSION, createDeviceAdapter } = require("./contract");

function createReferenceVendorAdapter() {
  return createDeviceAdapter({
    manifest: {
      contractVersion: ADAPTER_CONTRACT_VERSION,
      id: "reference",
      displayName: "Reference Vendor",
      aliases: ["reference"],
      implementationStatus: "identity-only"
    },
    // Protocols, models, firmware and operations remain unverified until their
    // individual roadmap gates approve evidence and implementation.
    operations: {}
  });
}

module.exports = {
  createReferenceVendorAdapter
};
