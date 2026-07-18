const assert = require("assert");
const { getSchemaStatements } = require("./schema-loader");

const statements = getSchemaStatements();
const triggers = statements.filter((statement) => /CREATE\s+TRIGGER/i.test(statement));

assert.strictEqual(triggers.length, 4, "all inventory system-group triggers should remain intact");
triggers.forEach((trigger) => {
  assert.match(trigger, /\bBEGIN\b/i);
  assert.match(trigger, /\bEND;\s*$/i);
});
assert.ok(
  !statements.some((statement) => /^END\b/i.test(statement)),
  "trigger bodies must not be split into standalone statements"
);

console.log("ok - schema loader preserves system-group triggers");
