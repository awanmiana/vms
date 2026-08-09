const fs = require("fs");
const path = require("path");
const { DECLARED_RECORDS, DECLARED_TABLES, emptyStore } = require("./default-data");
const { formatDateTime } = require("./datetime");

const declaredTables = new Set(DECLARED_TABLES);
const declaredRecords = new Set(DECLARED_RECORDS);
const declaredStoreKeys = new Set(["version", ...DECLARED_TABLES, ...DECLARED_RECORDS]);

class FileDatabaseError extends Error {
  constructor(message, code) {
    super(message);
    this.name = new.target.name;
    this.code = code;
  }
}

class FileDatabasePersistenceError extends FileDatabaseError {
  constructor(message = "Durable persistence is unavailable; the requested change was not saved.", cause) {
    super(message, "PERSISTENCE_UNAVAILABLE");
    if (cause) this.cause = cause;
  }
}

class FileDatabaseSchemaError extends FileDatabaseError {
  constructor(message, cause) {
    super(message, "PERSISTENCE_SCHEMA_INVALID");
    if (cause) this.cause = cause;
  }
}

function clone(value) {
  return value === undefined ? undefined : JSON.parse(JSON.stringify(value));
}

function assertDeclaredTable(name) {
  if (!declaredTables.has(name)) {
    throw new FileDatabaseSchemaError(`Undeclared persistence table: ${name}.`);
  }
}

function assertDeclaredRecord(name) {
  if (!declaredRecords.has(name)) {
    throw new FileDatabaseSchemaError(`Undeclared persistence record: ${name}.`);
  }
}

function validateStore(raw) {
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) {
    throw new FileDatabaseSchemaError("The persistence document must be an object.");
  }
  for (const key of Object.keys(raw)) {
    if (!declaredStoreKeys.has(key)) {
      throw new FileDatabaseSchemaError(`Undeclared persistence field: ${key}.`);
    }
  }
  for (const name of DECLARED_TABLES) {
    if (raw[name] !== undefined && !Array.isArray(raw[name])) {
      throw new FileDatabaseSchemaError(`Persistence table ${name} must be an array.`);
    }
  }
  for (const name of DECLARED_RECORDS) {
    if (raw[name] !== undefined && (!raw[name] || typeof raw[name] !== "object" || Array.isArray(raw[name]))) {
      throw new FileDatabaseSchemaError(`Persistence record ${name} must be an object.`);
    }
  }
  return { ...emptyStore(), ...clone(raw) };
}

class FileDatabase {
  constructor(filePath = path.join(__dirname, "vms-dev-db.json")) {
    this.filePath = filePath;
    this.store = emptyStore();
    this.memoryOnly = false;
    this.persistenceFallback = null;
  }

  load() {
    if (!fs.existsSync(this.filePath)) {
      this.save();
      return this.store;
    }

    try {
      const raw = fs.readFileSync(this.filePath, "utf8");
      this.store = validateStore(JSON.parse(raw));
    } catch (error) {
      if (error instanceof FileDatabaseError) throw error;
      throw new FileDatabaseSchemaError("The persistence document could not be loaded.", error);
    }
    return this.store;
  }

  save() {
    this.persistCandidate(this.store);
    return { durable: !this.memoryOnly, fallback: false };
  }

  markPersistenceUnavailable(error) {
    this.memoryOnly = true;
    this.persistenceFallback = {
      code: String(error?.code || "PERSISTENCE_UNAVAILABLE"),
      message: String(error?.message || "Durable persistence is unavailable."),
      detectedAt: formatDateTime()
    };
  }

  table(name) {
    assertDeclaredTable(name);
    return this.store[name];
  }

  snapshot(names = DECLARED_TABLES) {
    const selected = {};
    for (const name of names) {
      assertDeclaredTable(name);
      selected[name] = clone(this.store[name]);
    }
    return selected;
  }

  record(name) {
    assertDeclaredRecord(name);
    return clone(this.store[name]);
  }

  persistCandidate(candidate) {
    if (this.persistenceFallback) {
      throw new FileDatabasePersistenceError(undefined, this.persistenceFallback);
    }
    validateStore(candidate);
    if (this.memoryOnly) return;

    const targetPath = path.resolve(this.filePath);
    const directory = path.dirname(targetPath);
    const tempPath = path.join(directory, `.${path.basename(targetPath)}.${process.pid}.${Date.now()}.tmp`);
    fs.mkdirSync(directory, { recursive: true });
    try {
      const handle = fs.openSync(tempPath, "wx");
      try {
        fs.writeFileSync(handle, `${JSON.stringify(candidate, null, 2)}\n`, "utf8");
        fs.fsyncSync(handle);
      } finally {
        fs.closeSync(handle);
      }
      fs.renameSync(tempPath, targetPath);
    } catch (error) {
      try {
        if (fs.existsSync(tempPath)) fs.unlinkSync(tempPath);
      } catch {
        // Preserve the original persistence error.
      }
      this.markPersistenceUnavailable(error);
      throw new FileDatabasePersistenceError(undefined, error);
    }
  }

  transaction(mutator) {
    if (typeof mutator !== "function") {
      throw new FileDatabaseSchemaError("A persistence transaction requires a mutator function.");
    }
    if (this.persistenceFallback) {
      throw new FileDatabasePersistenceError(undefined, this.persistenceFallback);
    }
    const workingStore = clone(this.store);
    const transaction = {
      table(name) {
        assertDeclaredTable(name);
        return workingStore[name];
      },
      record(name) {
        assertDeclaredRecord(name);
        return clone(workingStore[name]);
      },
      setRecord(name, value) {
        assertDeclaredRecord(name);
        workingStore[name] = clone(value);
      }
    };
    const result = mutator(transaction);
    if (result && typeof result.then === "function") {
      throw new FileDatabaseSchemaError("FileDatabase transactions must be synchronous.");
    }
    this.persistCandidate(workingStore);
    this.store = workingStore;
    return result;
  }

  replaceTables(tables, records = {}) {
    if (!tables || typeof tables !== "object" || Array.isArray(tables)) {
      throw new FileDatabaseSchemaError("Replacement tables must be an object.");
    }
    return this.transaction((tx) => {
      for (const [name, rows] of Object.entries(tables)) {
        assertDeclaredTable(name);
        if (!Array.isArray(rows)) {
          throw new FileDatabaseSchemaError(`Persistence table ${name} must be an array.`);
        }
        const target = tx.table(name);
        target.splice(0, target.length, ...clone(rows));
      }
      for (const [name, value] of Object.entries(records)) tx.setRecord(name, value);
    });
  }

  upsert(tableName, row) {
    this.transaction((tx) => {
      const table = tx.table(tableName);
      const index = table.findIndex((item) => item.id === row.id);
      if (index >= 0) {
        table[index] = { ...table[index], ...row, updatedAt: formatDateTime() };
      } else {
        const now = formatDateTime();
        table.push({ ...row, createdAt: now, updatedAt: now });
      }
    });
    return row;
  }

  delete(tableName, id) {
    this.transaction((tx) => {
      const table = tx.table(tableName);
      const index = table.findIndex((item) => item.id === id);
      if (index >= 0) table.splice(index, 1);
    });
  }
}

module.exports = {
  FileDatabaseError,
  FileDatabase,
  FileDatabasePersistenceError,
  FileDatabaseSchemaError
};
