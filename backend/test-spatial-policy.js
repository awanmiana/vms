const assert = require("assert");
const fs = require("fs");
const path = require("path");
const vm = require("vm");
const { resolveTierByZone, shouldApplyTierChange } = require("./media-policy");

const source = fs.readFileSync(path.join(__dirname, "..", "spatial-canvas.js"), "utf8");
const sandbox = {
  window: {},
  performance: { now: () => 0 },
  requestAnimationFrame: () => 0,
  cancelAnimationFrame: () => {}
};
vm.runInNewContext(source, sandbox);

const frontendPolicy = sandbox.window.SpatialCanvasPolicy;

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

run("frontend spatial tier policy matches backend policy", () => {
  const zones = ["focus", "peripheral", "prewarm", "offscreen"];
  const zoomLevels = ["site", "wing", "room", "unknown"];
  const booleans = [false, true];

  zones.forEach((zone) => {
    zoomLevels.forEach((zoomLevel) => {
      booleans.forEach((isFocused) => {
        booleans.forEach((isTracking) => {
          const context = { zone, zoomLevel, isFocused, isTracking };
          assert.strictEqual(frontendPolicy.resolveTierByZone(context), resolveTierByZone(context), JSON.stringify(context));
        });
      });
    });
  });
});

run("frontend tier debounce policy matches backend policy", () => {
  const tiers = ["paused", "thumb", "sub", "main"];
  const settleTimes = [0, 299, 300, 600];

  tiers.forEach((fromTier) => {
    tiers.forEach((toTier) => {
      settleTimes.forEach((msSincePanSettled) => {
        const context = { fromTier, toTier, msSincePanSettled };
        assert.strictEqual(frontendPolicy.shouldApplyTierChange(context), shouldApplyTierChange(context), JSON.stringify(context));
      });
    });
  });
});

run("frontend tile grid layout creates non-overlapping camera tiles", () => {
  const cameras = Array.from({ length: 13 }, (_, index) => ({ id: `cam-${index + 1}` }));
  const laidOut = sandbox.window.SpatialCanvasLayout.layoutGrid(cameras);
  assert.strictEqual(laidOut.length, 13);
  assert.strictEqual(new Set(laidOut.map((camera) => `${camera.worldX},${camera.worldY}`)).size, 13);

  for (let i = 0; i < laidOut.length; i += 1) {
    for (let j = i + 1; j < laidOut.length; j += 1) {
      const dx = Math.abs(laidOut[i].worldX - laidOut[j].worldX);
      const dy = Math.abs(laidOut[i].worldY - laidOut[j].worldY);
      // TILE_GAP is now 0 (contiguous grid, like the VMS reference), so
      // adjacent tiles are expected to touch exactly at 320/190px spacing --
      // that's "not overlapping", not a bug.
      assert.ok(dx >= 320 || dy >= 190, `tiles overlap: ${laidOut[i].id}, ${laidOut[j].id}`);
    }
  }
});

run("frontend tile grid layout honors the selected division", () => {
  const cameras = [{ id: "cam-1" }, { id: "cam-2" }, { id: "cam-3" }];
  const laidOut = sandbox.window.SpatialCanvasLayout.layoutGrid(cameras, 4);
  const uniqueX = new Set(laidOut.map((camera) => camera.worldX));
  const uniqueY = new Set(laidOut.map((camera) => camera.worldY));
  assert.strictEqual(uniqueX.size, 2);
  assert.strictEqual(uniqueY.size, 2);
});

run("bandwidth cap: focused camera always keeps a slot even if far from center", () => {
  const { selectStreamingCandidates } = frontendPolicy;
  const candidates = [
    { camera: { id: "far-but-focused" }, state: { isFocused: true, isTracking: false }, zone: "peripheral", dist: 900 },
    { camera: { id: "close-1" }, state: { isFocused: false, isTracking: false }, zone: "focus", dist: 10 },
    { camera: { id: "close-2" }, state: { isFocused: false, isTracking: false }, zone: "focus", dist: 20 },
    { camera: { id: "close-3" }, state: { isFocused: false, isTracking: false }, zone: "peripheral", dist: 30 }
  ];

  const result = Array.from(selectStreamingCandidates(candidates, 2));
  const active = result.filter((entry) => entry.withinBudget).map((entry) => entry.camera.id);
  assert.deepStrictEqual(active, ["far-but-focused", "close-1"]);
});

run("bandwidth cap: with no focus, closest-to-center wins the available slots", () => {
  const { selectStreamingCandidates } = frontendPolicy;
  const candidates = [
    { camera: { id: "far" }, state: { isFocused: false, isTracking: false }, zone: "peripheral", dist: 500 },
    { camera: { id: "mid" }, state: { isFocused: false, isTracking: false }, zone: "peripheral", dist: 100 },
    { camera: { id: "near" }, state: { isFocused: false, isTracking: false }, zone: "focus", dist: 5 }
  ];

  const result = Array.from(selectStreamingCandidates(candidates, 2));
  const active = result.filter((entry) => entry.withinBudget).map((entry) => entry.camera.id);
  assert.deepStrictEqual(active, ["near", "mid"]);
  const excluded = result.filter((entry) => !entry.withinBudget).map((entry) => entry.camera.id);
  assert.deepStrictEqual(excluded, ["far"]);
});

run("bandwidth cap: a cap larger than the candidate count keeps everyone active", () => {
  const { selectStreamingCandidates } = frontendPolicy;
  const candidates = [
    { camera: { id: "a" }, state: { isFocused: false, isTracking: false }, zone: "focus", dist: 1 },
    { camera: { id: "b" }, state: { isFocused: false, isTracking: false }, zone: "peripheral", dist: 2 }
  ];
  const result = Array.from(selectStreamingCandidates(candidates, 10));
  assert.strictEqual(result.every((entry) => entry.withinBudget), true);
});

if (process.exitCode) {
  console.error("\nSpatial policy tests failed.");
} else {
  console.log("\nAll spatial policy tests passed.");
}
