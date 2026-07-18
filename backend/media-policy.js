const TIER_BITRATES = {
  paused: 0,
  thumb: 256,
  sub: 1000,
  main: 4000
};

const TIER_RANK = {
  paused: 0,
  thumb: 1,
  sub: 2,
  main: 3
};

const VALID_ZONES = new Set(["focus", "peripheral", "prewarm", "offscreen"]);
const VALID_ZOOM_LEVELS = new Set(["site", "wing", "room"]);

function resolveTier(context = {}) {
  if (context.zone) {
    return resolveTierByZone(context);
  }
  return resolveTierByGrid(context);
}

function resolveTierByGrid(context) {
  if (context.isVisible === false) return "paused";
  if (context.isTracking) return "main";
  if (context.isFocused) return "main";
  if (context.paneContext === "playback" && context.tileCount >= 4) return "sub";
  if (context.tileCount >= 9) return "thumb";
  if (context.tileCount >= 2) return "sub";
  return "main";
}

function resolveTierByZone(context) {
  const zone = VALID_ZONES.has(context.zone) ? context.zone : "offscreen";
  const zoomLevel = VALID_ZOOM_LEVELS.has(context.zoomLevel) ? context.zoomLevel : "room";

  if (context.isTracking) return "main";
  if (context.isFocused) return "main";
  if (zone === "offscreen") return "paused";
  if (zone === "prewarm") return "thumb";

  const baseTier = zone === "focus" ? "main" : "thumb";
  return applyZoomCap(baseTier, zoomLevel);
}

function applyZoomCap(baseTier, zoomLevel) {
  const cap = {
    site: "paused",
    wing: "thumb",
    room: "main"
  }[zoomLevel];

  if (TIER_RANK[cap] < TIER_RANK[baseTier]) return cap;
  return baseTier;
}

function estimateBitrateKbps(tier) {
  return TIER_BITRATES[tier] || 0;
}

function canOpenTier({ tier, deviceUsage, deviceLimits }) {
  if (tier === "main") {
    return deviceUsage.main < deviceLimits.maxConcurrentMainstream;
  }
  if (tier === "sub" || tier === "thumb") {
    return deviceUsage.sub < deviceLimits.maxConcurrentSubstream;
  }
  return true;
}

function shouldApplyTierChange({ fromTier, toTier, msSincePanSettled = 0, minPromoteDwellMs = 300 }) {
  if (fromTier === toTier) return false;

  const isDowngrade = (TIER_RANK[toTier] ?? 0) < (TIER_RANK[fromTier] ?? 0);
  if (isDowngrade) return true;

  return msSincePanSettled >= minPromoteDwellMs;
}

module.exports = {
  canOpenTier,
  estimateBitrateKbps,
  resolveTier,
  resolveTierByZone,
  shouldApplyTierChange,
  TIER_RANK
};
