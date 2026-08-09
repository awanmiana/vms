function device(overrides = {}) {
  return {
    id: "dev-test",
    name: "Test Recorder",
    type: "NVR",
    vendor: "Test Vendor",
    host: "192.168.50.10",
    port: 8000,
    channelCount: 2,
    status: "online",
    ...overrides
  };
}

module.exports = { device };
