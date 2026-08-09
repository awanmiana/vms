(function mediaPolicyModule(root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.VmsMediaPolicy = api;
})(typeof globalThis !== "undefined" ? globalThis : this, function createMediaPolicy() {
  "use strict";

  const TIER_BITRATES = Object.freeze({ paused: 0, thumb: 256, sub: 1000, main: 4000 });
  const TIER_RANK = Object.freeze({ paused: 0, thumb: 1, sub: 2, main: 3 });
  const VALID_ZONES = new Set(["focus", "peripheral", "prewarm", "offscreen"]);
  const VALID_ZOOM_LEVELS = new Set(["site", "wing", "room"]);
  const DEFAULT_STREAM_PROFILES = Object.freeze([
    Object.freeze({ tier: "thumb", resolution: "480p", fps: 6, bitrateKbps: 256, codec: "H.264" }),
    Object.freeze({ tier: "sub", resolution: "720p", fps: 15, bitrateKbps: 1000, codec: "H.264" }),
    Object.freeze({ tier: "main", resolution: "native", fps: 25, bitrateKbps: 4000, codec: "H.264/H.265" })
  ]);

  function resolveTier(context = {}) {
    return context.zone ? resolveTierByZone(context) : resolveTierByGrid(context);
  }

  function resolveTierByGrid(context = {}) {
    if (context.isVisible === false) return "paused";
    if (context.isTracking || context.isFocused) return "main";
    if (context.paneContext === "playback" && context.tileCount >= 4) return "sub";
    if (context.tileCount >= 9) return "thumb";
    if (context.tileCount >= 2) return "sub";
    return "main";
  }

  function resolveTierByZone(context = {}) {
    const zone = VALID_ZONES.has(context.zone) ? context.zone : "offscreen";
    const zoomLevel = VALID_ZOOM_LEVELS.has(context.zoomLevel) ? context.zoomLevel : "room";
    if (context.isTracking || context.isFocused) return "main";
    if (zone === "offscreen") return "paused";
    if (zone === "prewarm") return "thumb";
    return applyZoomCap(zone === "focus" ? "main" : "thumb", zoomLevel);
  }

  function applyZoomCap(baseTier, zoomLevel) {
    const cap = { site: "paused", wing: "thumb", room: "main" }[zoomLevel] || "main";
    return TIER_RANK[cap] < TIER_RANK[baseTier] ? cap : baseTier;
  }

  function estimateBitrateKbps(tier) {
    return TIER_BITRATES[tier] || 0;
  }

  function canOpenTier({ tier, deviceUsage, deviceLimits }) {
    if (tier === "main") return deviceUsage.main < deviceLimits.maxConcurrentMainstream;
    if (tier === "sub" || tier === "thumb") return deviceUsage.sub < deviceLimits.maxConcurrentSubstream;
    return true;
  }

  function shouldApplyTierChange({ fromTier, toTier, msSincePanSettled = 0, minPromoteDwellMs = 300 }) {
    if (fromTier === toTier) return false;
    if ((TIER_RANK[toTier] ?? 0) < (TIER_RANK[fromTier] ?? 0)) return true;
    return msSincePanSettled >= minPromoteDwellMs;
  }

  function defaultStreamProfiles(cameraId) {
    return DEFAULT_STREAM_PROFILES.map((profile) => ({
      id: `${cameraId}-${profile.tier}`,
      cameraId,
      ...profile,
      isAvailable: true
    }));
  }

  return Object.freeze({
    DEFAULT_STREAM_PROFILES,
    TIER_BITRATES,
    TIER_RANK,
    canOpenTier,
    defaultStreamProfiles,
    estimateBitrateKbps,
    resolveTier,
    resolveTierByGrid,
    resolveTierByZone,
    shouldApplyTierChange
  });
});
