const assert = require("assert");
const path = require("path");
const { FileDatabase } = require("./file-db");
const { CommandExecutor, createCommand } = require("./commands");
const { RegexCommandParser, createAliasResolver } = require("./voice-regex");

function freshDb() {
  const db = new FileDatabase(path.join(__dirname, "test-db-unused.json"));
  db.memoryOnly = true;
  db.load();
  return db;
}

function seedCamera(db, { id, displayName }) {
  db.upsert("cameras", { id, displayName, area: "Test", floor: "1", direction: "n/a" });
  return db.table("cameras").find((camera) => camera.id === id);
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

run("resolves an aliased camera name and opens it", () => {
  const db = freshDb();
  seedCamera(db, { id: "cam-1", displayName: "NVR-1-CAM-01" });
  db.upsert("voiceAliases", {
    id: "alias-1",
    entityType: "camera",
    entityId: "cam-1",
    alias: "front door",
    normalizedAlias: "front door"
  });

  const parser = new RegexCommandParser(createAliasResolver(db));
  const executor = new CommandExecutor(db, {});

  const commands = parser.parse("open front door");
  assert.strictEqual(commands.length, 1);
  assert.strictEqual(commands[0].intent, "OPEN_CAMERA");
  assert.strictEqual(commands[0].confidence, 0.9);

  const result = executor.execute(commands[0]);
  assert.strictEqual(result.status, "executed");
  assert.strictEqual(result.camera.id, "cam-1");
});

run("splits a 3-clause compound utterance into 3 ordered commands", () => {
  const db = freshDb();
  seedCamera(db, { id: "cam-1", displayName: "NVR-1-CAM-01" });
  db.upsert("voiceAliases", {
    id: "alias-1",
    entityType: "camera",
    entityId: "cam-1",
    alias: "front door",
    normalizedAlias: "front door"
  });

  const parser = new RegexCommandParser(createAliasResolver(db));
  const commands = parser.parse("open front door then start tracking front door and pause");

  assert.strictEqual(commands.length, 3);
  assert.deepStrictEqual(
    commands.map((command) => command.intent),
    ["OPEN_CAMERA", "START_TRACKING", "PAUSE_SESSION"]
  );
});

run("an unresolved camera name is rejected without throwing, flagged lower-confidence", () => {
  const db = freshDb();
  seedCamera(db, { id: "cam-1", displayName: "NVR-1-CAM-01" });

  const parser = new RegexCommandParser(createAliasResolver(db));
  const executor = new CommandExecutor(db, {});

  const commands = parser.parse("open loading dock");
  assert.strictEqual(commands.length, 1);
  assert.strictEqual(commands[0].confidence, 0.72);

  const result = executor.execute(commands[0]);
  assert.strictEqual(result.status, "rejected");
  assert.strictEqual(result.reason, "Camera not found");
});

run("an unrecognized phrase produces zero commands, not a throw", () => {
  const db = freshDb();
  const parser = new RegexCommandParser(createAliasResolver(db));
  const commands = parser.parse("do a barrel roll");
  assert.strictEqual(commands.length, 0);
});

run("STOP_ALL asks for confirmation and does not mutate existing session state", () => {
  const db = freshDb();
  seedCamera(db, { id: "cam-1", displayName: "NVR-1-CAM-01" });
  db.upsert("voiceAliases", {
    id: "alias-1",
    entityType: "camera",
    entityId: "cam-1",
    alias: "front door",
    normalizedAlias: "front door"
  });

  const parser = new RegexCommandParser(createAliasResolver(db));
  const executor = new CommandExecutor(db, {});

  const [startCommand] = parser.parse("start tracking front door");
  executor.execute(startCommand);
  const sessionBefore = db.table("trackingSessions")[0];
  assert.strictEqual(sessionBefore.status, "active");

  const [stopCommand] = parser.parse("close all");
  const result = executor.execute(stopCommand);
  assert.strictEqual(result.status, "needs_confirmation");

  const sessionAfter = db.table("trackingSessions")[0];
  assert.strictEqual(sessionAfter.status, "active");
});

run("START_TRACKING logs a breadcrumb with canvas position when the camera is placed", () => {
  const db = freshDb();
  seedCamera(db, { id: "cam-1", displayName: "NVR-1-CAM-01" });
  db.upsert("cameraPositions", { id: "pos-1", cameraId: "cam-1", canvasId: "canvas-1", xPct: 42, yPct: 17 });

  const executor = new CommandExecutor(db, {});
  const command = createCommand({
    intent: "START_TRACKING",
    target: { type: "camera", id: "cam-1" },
    source: "ui_click"
  });

  const result = executor.execute(command);
  assert.strictEqual(result.status, "executed");

  const crumb = db.table("sessionBreadcrumbs").find((row) => row.sessionId === result.session.id);
  assert.ok(crumb, "breadcrumb should exist");
  assert.strictEqual(crumb.canvasXPct, 42);
  assert.strictEqual(crumb.canvasYPct, 17);
});

if (process.exitCode) {
  console.error("\nSome tests failed.");
} else {
  console.log("\nAll tests passed.");
}
