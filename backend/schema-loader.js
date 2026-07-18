const fs = require("fs");
const path = require("path");

const schemaPath = path.join(__dirname, "..", "docs", "sqlite-schema.sql");

function loadSchemaSql() {
  return fs.readFileSync(schemaPath, "utf8");
}

function splitSqlStatements(sql) {
  const statements = [];
  let buffer = [];
  let inTrigger = false;

  String(sql || "")
    .split(/\r?\n/)
    .forEach((line) => {
      buffer.push(line);
      const trimmed = line.trim();

      if (!inTrigger && /^CREATE\s+TRIGGER\b/i.test(trimmed)) {
        inTrigger = true;
      }

      if (inTrigger) {
        if (/^END;\s*$/i.test(trimmed)) {
          const statement = buffer.join("\n").trim();
          if (statement) statements.push(statement);
          buffer = [];
          inTrigger = false;
        }
        return;
      }

      if (/;\s*$/.test(trimmed)) {
        const statement = buffer.join("\n").trim().replace(/;\s*$/, "");
        if (statement) statements.push(statement);
        buffer = [];
      }
    });

  const remainder = buffer.join("\n").trim();
  if (remainder) statements.push(remainder);
  return statements;
}

function getSchemaStatements() {
  return splitSqlStatements(loadSchemaSql());
}

module.exports = {
  getSchemaStatements,
  loadSchemaSql,
  splitSqlStatements
};
