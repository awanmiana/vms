const path = require("path");
const { spawnSync } = require("child_process");

const tests = [
  "test-device-model.js",
  "test-workspaces.js",
  "test-operational-ui.js",
  "backend/test-device-onboarding.js",
  "backend/test-api-server.js",
  "backend/test-schema-loader.js",
  "backend/test-device-adapter-contract.js",
  "backend/test-resilience-availability.js",
  "backend/test-resilience-operations.js",
  "backend/test-resilience-health.js",
  "backend/test-resilience-recovery-priority.js",
  "backend/test-resilience-service.js",
  "backend/test-device-integration.js",
  "backend/test-commands.js",
  "backend/test-compliance-services.js",
  "backend/test-spatial-policy.js"
];

for (const test of tests) {
  const result = spawnSync(process.execPath, [path.join(__dirname, test)], {
    cwd: __dirname,
    stdio: "inherit"
  });
  if (result.status !== 0) process.exit(result.status || 1);
}

console.log("\nAll VMS regression tests passed.");
