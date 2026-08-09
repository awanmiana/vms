const assert = require("assert");
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const root = path.join(__dirname, "..");

function createAppTestContext({ Date: DateOverride, elements = {}, navigator = {}, storage = new Map() } = {}) {
  const context = {
    __VMS_TEST_MODE__: true,
    console,
    ...(DateOverride ? { Date: DateOverride } : {}),
    navigator,
    localStorage: {
      getItem(key) { return storage.has(key) ? storage.get(key) : null; },
      setItem(key, value) { storage.set(key, String(value)); },
      removeItem(key) { storage.delete(key); }
    },
    window: {
      location: { hash: "" },
      history: { replaceState() {}, pushState() {} }
    },
    document: {
      getElementById(id) { return elements[id] || null; },
      querySelector() { return null; },
      querySelectorAll() { return []; }
    }
  };
  context.globalThis = context;
  return context;
}

function loadAppTestApi(options = {}) {
  const context = options.context || createAppTestContext(options);
  vm.createContext(context);
  for (const source of [
    "shared/core.js",
    "shared/media-policy.js",
    "shared/inventory/contract.js",
    "app.js"
  ]) {
    vm.runInContext(fs.readFileSync(path.join(root, source), "utf8"), context, { filename: source });
  }
  assert(context.__VMS_TEST_API__, "app.js should expose the VMS test API");
  return { api: context.__VMS_TEST_API__, context };
}

module.exports = { createAppTestContext, loadAppTestApi };
