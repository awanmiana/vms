const assert = require("assert");
const fs = require("fs");
const os = require("os");
const path = require("path");
const {
  FileDatabase,
  FileDatabasePersistenceError,
  FileDatabaseSchemaError
} = require("./file-db");

let failures = 0;

function run(name, test) {
  try {
    test();
    console.log(`ok - ${name}`);
  } catch (error) {
    failures += 1;
    console.error(`FAIL - ${name}`);
    console.error(error);
  }
}

function withTempDatabase(test) {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), "vms-file-db-"));
  const filePath = path.join(directory, "store.json");
  try {
    test({ directory, filePath });
  } finally {
    for (const entry of fs.readdirSync(directory)) fs.unlinkSync(path.join(directory, entry));
    fs.rmdirSync(directory);
  }
}

run("declared tables reject accidental schema drift", () => {
  const db = new FileDatabase("unused-file-db-test.json");
  db.memoryOnly = true;
  assert.throws(() => db.table("inventedRows"), (error) =>
    error instanceof FileDatabaseSchemaError && error.code === "PERSISTENCE_SCHEMA_INVALID");
  assert.throws(() => db.replaceTables({ inventedRows: [] }), /Undeclared persistence table/);
});

run("a failed transaction leaves every table unchanged", () => {
  const db = new FileDatabase("unused-file-db-test.json");
  db.memoryOnly = true;
  db.upsert("devices", { id: "device-before", name: "Before" });
  const before = db.snapshot(["devices", "groups"]);
  assert.throws(() => db.transaction((tx) => {
    tx.table("devices").push({ id: "device-after" });
    tx.table("groups").push({ id: "group-after" });
    throw new Error("reject the unit of work");
  }), /reject the unit of work/);
  assert.deepStrictEqual(db.snapshot(["devices", "groups"]), before);
});

run("atomic replacement commits tables and metadata as one durable document", () => {
  withTempDatabase(({ directory, filePath }) => {
    const db = new FileDatabase(filePath);
    db.load();
    db.replaceTables({
      devices: [{ id: "device-committed" }],
      cameras: [{ id: "camera-committed", deviceId: "device-committed" }],
      groups: []
    }, {
      inventoryState: { initialized: true, version: 2 }
    });

    const reopened = new FileDatabase(filePath);
    reopened.load();
    assert.deepStrictEqual(reopened.snapshot(["devices", "cameras", "groups"]), {
      devices: [{ id: "device-committed" }],
      cameras: [{ id: "camera-committed", deviceId: "device-committed" }],
      groups: []
    });
    assert.deepStrictEqual(reopened.record("inventoryState"), { initialized: true, version: 2 });
    assert.deepStrictEqual(fs.readdirSync(directory), ["store.json"]);
  });
});

run("an unavailable durable adapter reports a typed error and rolls back memory", () => {
  const db = new FileDatabase("unused-file-db-test.json");
  db.memoryOnly = true;
  db.upsert("devices", { id: "device-before" });
  const before = db.snapshot(["devices"]);
  db.markPersistenceUnavailable(new Error("simulated durable failure"));
  assert.throws(
    () => db.replaceTables({ devices: [{ id: "device-after" }] }),
    (error) => error instanceof FileDatabasePersistenceError
      && error.code === "PERSISTENCE_UNAVAILABLE"
      && /not saved/.test(error.message)
  );
  assert.deepStrictEqual(db.snapshot(["devices"]), before);
});

run("loading undeclared fields fails closed with a typed schema error", () => {
  withTempDatabase(({ filePath }) => {
    fs.writeFileSync(filePath, JSON.stringify({ version: 1, surpriseTable: [] }));
    const db = new FileDatabase(filePath);
    assert.throws(
      () => db.load(),
      (error) => error instanceof FileDatabaseSchemaError
        && error.code === "PERSISTENCE_SCHEMA_INVALID"
    );
  });
});

if (failures) {
  console.error(`\n${failures} file database test(s) failed.`);
  process.exitCode = 1;
} else {
  console.log("\nAll file database transaction tests passed.");
}
