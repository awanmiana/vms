(function sharedCoreModule(root, factory) {
  const cryptoProvider = typeof module === "object" && module.exports
    ? require("crypto")
    : root.crypto;
  const api = factory(cryptoProvider);
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.VmsSharedCore = api;
})(typeof globalThis !== "undefined" ? globalThis : this, function createSharedCore(cryptoProvider) {
  "use strict";

  class SharedContractError extends Error {
    constructor(code, message) {
      super(message);
      this.name = "SharedContractError";
      this.code = code;
    }
  }

  function padTimePart(value) {
    return String(value).padStart(2, "0");
  }

  function formatDate(date = new Date()) {
    return `${date.getFullYear()}-${padTimePart(date.getMonth() + 1)}-${padTimePart(date.getDate())}`;
  }

  function formatTime(date = new Date()) {
    return `${padTimePart(date.getHours())}:${padTimePart(date.getMinutes())}:${padTimePart(date.getSeconds())}`;
  }

  function formatDateTime(date = new Date()) {
    return `${formatDate(date)} ${formatTime(date)}`;
  }

  function stableStringify(value) {
    if (Array.isArray(value)) return `[${value.map(stableStringify).join(",")}]`;
    if (value && typeof value === "object") {
      const keys = Object.keys(value).sort();
      return `{${keys.map((key) => `${JSON.stringify(key)}:${stableStringify(value[key])}`).join(",")}}`;
    }
    return JSON.stringify(value);
  }

  function compareById(left, right) {
    return String(left?.id || "").localeCompare(String(right?.id || ""));
  }

  function stableSort(values, compare) {
    if (!Array.isArray(values)) throw new SharedContractError("INVALID_SORT_INPUT", "Stable sort input must be an array.");
    if (typeof compare !== "function") throw new SharedContractError("INVALID_SORT_COMPARATOR", "Stable sort requires a comparator.");
    return values
      .map((value, index) => ({ value, index }))
      .sort((left, right) => compare(left.value, right.value) || left.index - right.index)
      .map((entry) => entry.value);
  }

  function createId(prefix) {
    const normalizedPrefix = String(prefix || "").trim().toLowerCase();
    if (!/^[a-z][a-z0-9-]*$/.test(normalizedPrefix)) {
      throw new SharedContractError("INVALID_ID_PREFIX", `Invalid id prefix: ${prefix || "(empty)"}.`);
    }
    if (!cryptoProvider || typeof cryptoProvider.randomUUID !== "function") {
      throw new SharedContractError("UUID_UNAVAILABLE", "A cryptographic random UUID provider is required.");
    }
    return `${normalizedPrefix}-${cryptoProvider.randomUUID()}`;
  }

  return Object.freeze({
    SharedContractError,
    compareById,
    createId,
    formatDate,
    formatDateTime,
    formatTime,
    stableSort,
    stableStringify
  });
});
