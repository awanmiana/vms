const fs = require("fs");
const path = require("path");
const { emptyStore } = require("./default-data");
const { formatDateTime } = require("./datetime");

class FileDatabasePersistenceError extends Error {
  constructor(message = "Durable persistence is unavailable.") {
    super(message);
    this.name = "FileDatabasePersistenceError";
    this.code = "PERSISTENCE_UNAVAILABLE";
  }
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

    const raw = fs.readFileSync(this.filePath, "utf8");
    this.store = { ...emptyStore(), ...JSON.parse(raw) };
    return this.store;
  }

  save() {
    if (this.memoryOnly) {
      return {
        durable: false,
        fallback: Boolean(this.persistenceFallback)
      };
    }
    fs.mkdirSync(path.dirname(this.filePath), { recursive: true });
    try {
      fs.writeFileSync(this.filePath, `${JSON.stringify(this.store, null, 2)}\n`);
      return { durable: true, fallback: false };
    } catch (error) {
      if (error.code !== "EBADF" && error.code !== "ENOENT") {
        throw error;
      }
      this.markPersistenceUnavailable(error);
      return { durable: false, fallback: true };
    }
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
    if (!Array.isArray(this.store[name])) {
      this.store[name] = [];
    }
    return this.store[name];
  }

  upsert(tableName, row) {
    const table = this.table(tableName);
    const previousTable = table.map((item) => ({ ...item }));
    const index = table.findIndex((item) => item.id === row.id);
    if (index >= 0) {
      table[index] = { ...table[index], ...row, updatedAt: formatDateTime() };
    } else {
      const now = formatDateTime();
      table.push({ ...row, createdAt: now, updatedAt: now });
    }
    const persistence = this.save();
    if (persistence.fallback) {
      this.store[tableName] = previousTable;
      throw new FileDatabasePersistenceError(this.persistenceFallback?.message);
    }
    return row;
  }

  delete(tableName, id) {
    const table = this.table(tableName);
    const previousTable = table.map((item) => ({ ...item }));
    const next = table.filter((item) => item.id !== id);
    this.store[tableName] = next;
    const persistence = this.save();
    if (persistence.fallback) {
      this.store[tableName] = previousTable;
      throw new FileDatabasePersistenceError(this.persistenceFallback?.message);
    }
  }
}

module.exports = {
  FileDatabase,
  FileDatabasePersistenceError
};
