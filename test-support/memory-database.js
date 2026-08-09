const path = require("path");
const { FileDatabase } = require("../backend/file-db");

function createMemoryDatabase(name = "test") {
  const db = new FileDatabase(path.join(__dirname, `unused-${name}.json`));
  db.memoryOnly = true;
  db.load();
  return db;
}

module.exports = { createMemoryDatabase };
