#pragma once

// P3-15 — spatial-canvas viewport media policy (native increment 24). Scope:
// ../spatial-canvas-P3-15-proposal.md.
//
// A pure, Qt-free port of the reference prototype's `spatial-canvas.js` policy
// (the owner-directed "same or better" baseline): the same zones (focus /
// peripheral / prewarm / culled), the same zoom levels (site / wing / room)
// with the same tier caps, and the same anti-flap rule (downgrade immediately,
// promote only after the pan/zoom settles). The difference is WHERE the result
// goes: the prototype applied its tiers to a crude maxActiveStreams sort; here
// they feed the real governor, whose decode/memory/bandwidth admission decides
// honestly — and a culled tile becomes visible=false, i.e. genuinely not
// decoding (PausedOffscreen), not merely not drawn.
//
// Header-only and unit-tested headlessly by `vms_workspace --spatial-selftest`.

#include <algorithm>
#include <cmath>

namespace vms::spatial {

// The prototype's world-space tile geometry and interaction constants.
inline constexpr double kTileW = 320.0;
inline constexpr double kTileH = 190.0;
inline constexpr double kFocusRadiusFrac = 0.24;       // of min(view w, h)
inline constexpr double kPeripheralRadiusFrac = 0.55;  // of min(view w, h)
inline constexpr double kPrewarmMarginPx = 240.0;      // off-screen pre-warm band
inline constexpr int kSettleMs = 300;                  // promote-dwell after pan
inline constexpr double kMinZoom = 0.15;
inline constexpr double kMaxZoom = 6.0;

// Tier levels use the same ints the WorkspaceController API already speaks:
// 3=Main, 2=Sub, 1=Thumb, 0=Paused.
enum class Zone { Culled, Prewarm, Peripheral, Focus };
enum class ZoomLevel { Site, Wing, Room };

inline ZoomLevel ZoomLevelFor(double zoom) {
    if (zoom < 0.4) return ZoomLevel::Site;
    if (zoom < 0.9) return ZoomLevel::Wing;
    return ZoomLevel::Room;
}

inline const char* ZoomLevelName(ZoomLevel z) {
    switch (z) {
        case ZoomLevel::Site: return "site";
        case ZoomLevel::Wing: return "wing";
        case ZoomLevel::Room: return "room";
    }
    return "?";
}

// The prototype's default world placement: a zero-gap grid of 320x190 cells,
// ceil(sqrt(n)) columns, positions at cell centers.
inline void GridWorldPos(int index, int count, double& x, double& y) {
    const int columns = std::max(
        1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(
               std::max(1, count))))));
    const int col = index % columns;
    const int row = index / columns;
    x = col * kTileW + kTileW / 2.0;
    y = row * kTileH + kTileH / 2.0;
}

// Classify a tile by its on-screen position (tile center in screen px, half
// extents already scaled by the zoom). Mirrors the prototype's _zoneFor:
//   - fully outside the view + pre-warm margin  -> Culled
//   - inside the margin but off the visible view -> Prewarm
//   - within 24% of min dimension from center    -> Focus
//   - otherwise                                  -> Peripheral
inline Zone ZoneFor(double sx, double sy, double viewW, double viewH,
                    double halfW, double halfH) {
    const bool within = sx + halfW >= -kPrewarmMarginPx &&
                        sx - halfW <= viewW + kPrewarmMarginPx &&
                        sy + halfH >= -kPrewarmMarginPx &&
                        sy - halfH <= viewH + kPrewarmMarginPx;
    if (!within) return Zone::Culled;

    const double minDim = std::min(viewW, viewH);
    const double dist = std::hypot(sx - viewW / 2.0, sy - viewH / 2.0);
    if (dist <= minDim * kFocusRadiusFrac) return Zone::Focus;
    if (dist <= minDim * kPeripheralRadiusFrac) return Zone::Peripheral;
    if (sx + halfW < 0 || sx - halfW > viewW || sy + halfH < 0 ||
        sy - halfH > viewH)
        return Zone::Prewarm;
    return Zone::Peripheral;
}

// The prototype's resolveTierByZone, exactly: a focused camera is always Main
// (it bypasses the zoom cap, as in the reference); a culled tile is paused; a
// prewarm tile idles at Thumb; the focus zone earns Main, everything else
// Thumb; and the zoom level caps the result (site -> paused, wing -> thumb,
// room -> main).
inline int ResolveTierByZone(Zone zone, ZoomLevel zoomLevel, bool focused) {
    if (focused) return 3;
    if (zone == Zone::Culled) return 0;
    if (zone == Zone::Prewarm) return 1;
    const int base = (zone == Zone::Focus) ? 3 : 1;
    int cap = 3;
    switch (zoomLevel) {
        case ZoomLevel::Site: cap = 0; break;
        case ZoomLevel::Wing: cap = 1; break;
        case ZoomLevel::Room: cap = 3; break;
    }
    return std::min(cap, base);
}

// The prototype's shouldApplyTierChange, exactly: no-ops never apply, a
// downgrade applies immediately (cost must drop at once), and a promotion
// waits until the viewport has settled for kSettleMs (no flap mid-pan).
inline bool ShouldApplyTierChange(int fromLevel, int toLevel,
                                  int msSinceSettled,
                                  int minPromoteDwellMs = kSettleMs) {
    if (fromLevel == toLevel) return false;
    if (toLevel < fromLevel) return true;
    return msSinceSettled >= minPromoteDwellMs;
}

} // namespace vms::spatial
