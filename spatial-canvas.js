// spatial-canvas.js
//
// Real camera-tile canvas for the VMS live/playback surface. This is not a
// dot map: it renders desktop-VMS-style camera tiles on one pannable/zoomable canvas.
// Expensive stream state is still virtualized by viewport zone.

(function (global) {
  "use strict";

  const TIER_RANK = { paused: 0, thumb: 1, sub: 2, main: 3 };

  function resolveTierByZone({ zone, zoomLevel, isFocused, isTracking }) {
    if (isTracking) return "main";
    if (isFocused) return "main";
    if (zone === "offscreen") return "paused";
    if (zone === "prewarm") return "thumb";
    const baseTier = zone === "focus" ? "main" : "thumb";
    const cap = { site: "paused", wing: "thumb", room: "main" }[zoomLevel] || "main";
    return TIER_RANK[cap] < TIER_RANK[baseTier] ? cap : baseTier;
  }

  function shouldApplyTierChange({ fromTier, toTier, msSincePanSettled = 0, minPromoteDwellMs = 300 }) {
    if (fromTier === toTier) return false;
    const isDowngrade = (TIER_RANK[toTier] ?? 0) < (TIER_RANK[fromTier] ?? 0);
    if (isDowngrade) return true;
    return msSincePanSettled >= minPromoteDwellMs;
  }

  // Decides which candidate tiles are allowed to actually stream, given a
  // global cap. Focused/tracked cameras always keep a slot; the remaining
  // slots go to whichever candidates are closest to screen center. Returns
  // the same array with an added `withinBudget` boolean per entry, in
  // priority order (not original order).
  function selectStreamingCandidates(candidates, maxActiveStreams) {
    const sorted = [...candidates].sort((a, b) => {
      const aPriority = a.state.isFocused || a.state.isTracking ? 0 : 1;
      const bPriority = b.state.isFocused || b.state.isTracking ? 0 : 1;
      if (aPriority !== bPriority) return aPriority - bPriority;
      return a.dist - b.dist;
    });
    return sorted.map((entry, index) => ({ ...entry, withinBudget: index < maxActiveStreams }));
  }

  const TILE_WIDTH = 320;
  const TILE_HEIGHT = 190;
  const TILE_GAP = 0;
  const FOCUS_RADIUS_FRACTION = 0.24;
  const PERIPHERAL_RADIUS_FRACTION = 0.55;
  const PREWARM_MARGIN_PX = 240;
  const PAN_SETTLE_MS = 300;
  const TIER_LABEL = { paused: "Idle", thumb: "Preview", sub: "Live - SD", main: "Live - HD" };
  const TIER_ACCENT = { paused: "#555b66", thumb: "#4c8bf5", sub: "#f5a623", main: "#42d477" };

  function layoutGrid(cameras, division = cameras.length) {
    const slotCount = Math.max(1, Number(division) || cameras.length || 1);
    const columns = Math.max(1, Math.ceil(Math.sqrt(slotCount)));
    return cameras.map((camera, index) => {
      const col = index % columns;
      const row = Math.floor(index / columns);
      return {
        ...camera,
        worldX: col * (TILE_WIDTH + TILE_GAP) + TILE_WIDTH / 2,
        worldY: row * (TILE_HEIGHT + TILE_GAP) + TILE_HEIGHT / 2
      };
    });
  }

  function percentToWorld(cameras, worldWidth, worldHeight) {
    return cameras.map((camera) => ({
      ...camera,
      worldX: (camera.xPct / 100) * worldWidth,
      worldY: (camera.yPct / 100) * worldHeight
    }));
  }

  class SpatialCanvas {
    constructor(canvasEl, options = {}) {
      this.canvas = canvasEl;
      this.ctx = canvasEl.getContext("2d");
      this.onCameraSelect = options.onCameraSelect || (() => {});
      this.onCameraDoubleClick = options.onCameraDoubleClick || (() => {});
      this.onTierChange = options.onTierChange || (() => {});
      this.getZoomLevel = options.getZoomLevel || (() => this._zoomLevelFromScale());
      this.maxActiveStreams = options.maxActiveStreams || Infinity;
      this.division = options.division || 9;

      this.cameras = [];
      this.cameraState = new Map();
      this.snapshotCache = new Map();
      this.offsetX = 0;
      this.offsetY = 0;
      this.scale = 1;
      this.fitScaleX = 1;
      this.fitScaleY = 1;
      this._isPanning = false;
      this._lastPointer = null;
      this._pointerStart = null;
      this._panSettledTimer = null;
      this._resizeObserver = null;
      this._isSettled = true;
      this._lastSettleAt = performance.now();

      this._bindEvents();
      this._resizeToContainer();
      this._raf = requestAnimationFrame(() => this._renderLoop());
    }

    setCameras(cameras) {
      this.cameras = cameras;
      cameras.forEach((camera) => {
        if (!this.cameraState.has(camera.id)) {
          this.cameraState.set(camera.id, {
            tier: "paused",
            lastChangeAt: performance.now(),
            isFocused: false,
            isTracking: false
          });
        }
        if (camera.snapshotUrl && !this.snapshotCache.has(camera.id)) {
          const img = new Image();
          img.src = camera.snapshotUrl;
          this.snapshotCache.set(camera.id, img);
        }
      });
      this.setDivision(this.division);
      this._hasFramedOnce = true;
    }

    setMaxActiveStreams(n) {
      this.maxActiveStreams = Math.max(1, Number(n) || 6);
    }

    setDivision(n) {
      this.division = Math.max(1, Number(n) || 9);
      if (!this.canvas.width || !this.canvas.height) return;
      const columns = Math.max(1, Math.ceil(Math.sqrt(this.division)));
      const rows = Math.max(1, Math.ceil(this.division / columns));
      const contentWidth = columns * TILE_WIDTH;
      const contentHeight = rows * TILE_HEIGHT;
      this.fitScaleX = this.canvas.width / contentWidth;
      this.fitScaleY = this.canvas.height / contentHeight;
      this.scale = 1;
      this.offsetX = 0;
      this.offsetY = 0;
      this._markUnsettled();
    }

    focusCamera(cameraId, { tracking = false, center = true } = {}) {
      this.cameraState.forEach((state, id) => {
        state.isFocused = id === cameraId;
        if (id !== cameraId) state.isTracking = false;
      });
      const state = this.cameraState.get(cameraId);
      if (state) state.isTracking = tracking;
      const camera = this.cameras.find((item) => item.id === cameraId);
      if (camera && center) {
        this.offsetX = this.canvas.width / 2 - camera.worldX * this.fitScaleX * this.scale;
        this.offsetY = this.canvas.height / 2 - camera.worldY * this.fitScaleY * this.scale;
      }
    }

    frameCameras(cameraIds) {
      const selected = this.cameras.filter((camera) => cameraIds.includes(camera.id));
      if (!selected.length) return;
      this.setDivision(this.division);
    }

    destroy() {
      cancelAnimationFrame(this._raf);
      clearTimeout(this._panSettledTimer);
      this._unbindEvents();
    }

    _fitToContent(cameras = this.cameras) {
      if (!cameras.length || !this.canvas.width || !this.canvas.height) return;
      const xs = cameras.map((camera) => camera.worldX);
      const ys = cameras.map((camera) => camera.worldY);
      const minX = Math.min(...xs) - TILE_WIDTH / 2;
      const maxX = Math.max(...xs) + TILE_WIDTH / 2;
      const minY = Math.min(...ys) - TILE_HEIGHT / 2;
      const maxY = Math.max(...ys) + TILE_HEIGHT / 2;
      const contentW = Math.max(TILE_WIDTH, maxX - minX);
      const contentH = Math.max(TILE_HEIGHT, maxY - minY);
      const padding = 76;
      const scaleX = (this.canvas.width - padding) / contentW;
      const scaleY = (this.canvas.height - padding) / contentH;
      this.scale = Math.min(1, Math.max(0.2, Math.min(scaleX, scaleY)));
      this.offsetX = this.canvas.width / 2 - ((minX + maxX) / 2) * this.scale;
      this.offsetY = this.canvas.height / 2 - ((minY + maxY) / 2) * this.scale;
    }

    _bindEvents() {
      this._onPointerDown = (event) => {
        this.canvas.setPointerCapture?.(event.pointerId);
        this._isPanning = true;
        this._lastPointer = { x: event.clientX, y: event.clientY };
        this._pointerStart = { x: event.clientX, y: event.clientY };
        this.canvas.style.cursor = "grabbing";
        this._markUnsettled();
      };
      this._onPointerMove = (event) => {
        if (!this._isPanning || !this._lastPointer) return;
        this.offsetX += event.clientX - this._lastPointer.x;
        this.offsetY += event.clientY - this._lastPointer.y;
        this._lastPointer = { x: event.clientX, y: event.clientY };
        this._markUnsettled();
      };
      this._onPointerUp = (event) => {
        if (this._isPanning && this._pointerStart) {
          const moved = Math.hypot(event.clientX - this._pointerStart.x, event.clientY - this._pointerStart.y);
          if (moved < 3) this._handleClick(event);
        }
        this._isPanning = false;
        this._lastPointer = null;
        this._pointerStart = null;
        this.canvas.style.cursor = "grab";
      };
      this._onWheel = (event) => {
        event.preventDefault();
        const zoomFactor = event.deltaY < 0 ? 1.1 : 0.9;
        const rect = this.canvas.getBoundingClientRect();
        const cx = event.clientX - rect.left;
        const cy = event.clientY - rect.top;
        this.offsetX = cx - (cx - this.offsetX) * zoomFactor;
        this.offsetY = cy - (cy - this.offsetY) * zoomFactor;
        this.scale = Math.min(6, Math.max(0.15, this.scale * zoomFactor));
        this._markUnsettled();
      };
      this._onDblClick = (event) => {
        const rect = this.canvas.getBoundingClientRect();
        const hit = this._hitTest(event.clientX - rect.left, event.clientY - rect.top);
        if (hit) {
          this.onCameraDoubleClick(hit);
        }
      };
      this._onResize = () => {
        this._resizeToContainer();
        this.setDivision(this.division);
      };

      this.canvas.style.cursor = "grab";
      this.canvas.addEventListener("pointerdown", this._onPointerDown);
      window.addEventListener("pointermove", this._onPointerMove);
      window.addEventListener("pointerup", this._onPointerUp);
      this.canvas.addEventListener("wheel", this._onWheel, { passive: false });
      this.canvas.addEventListener("dblclick", this._onDblClick);
      window.addEventListener("resize", this._onResize);

      const ResizeObserverCtor = global.ResizeObserver;
      if (typeof ResizeObserverCtor === "function") {
        this._resizeObserver = new ResizeObserverCtor(this._onResize);
        this._resizeObserver.observe(this.canvas.parentElement || this.canvas);
      }
    }

    _unbindEvents() {
      this.canvas.removeEventListener("pointerdown", this._onPointerDown);
      window.removeEventListener("pointermove", this._onPointerMove);
      window.removeEventListener("pointerup", this._onPointerUp);
      this.canvas.removeEventListener("wheel", this._onWheel);
      this.canvas.removeEventListener("dblclick", this._onDblClick);
      window.removeEventListener("resize", this._onResize);
      this._resizeObserver?.disconnect();
      this._resizeObserver = null;
    }

    _handleClick(event) {
      const rect = this.canvas.getBoundingClientRect();
      const hit = this._hitTest(event.clientX - rect.left, event.clientY - rect.top);
      if (hit) this.onCameraSelect(hit);
    }

    _hitTest(screenX, screenY) {
      for (const camera of this.cameras) {
        const { x, y } = this._toScreen(camera);
        const halfW = (TILE_WIDTH / 2) * this.fitScaleX * this.scale;
        const halfH = (TILE_HEIGHT / 2) * this.fitScaleY * this.scale;
        if (screenX >= x - halfW && screenX <= x + halfW && screenY >= y - halfH && screenY <= y + halfH) {
          return camera;
        }
      }
      return null;
    }

    _markUnsettled() {
      this._isSettled = false;
      clearTimeout(this._panSettledTimer);
      this._panSettledTimer = setTimeout(() => {
        this._isSettled = true;
        this._lastSettleAt = performance.now();
      }, PAN_SETTLE_MS);
    }

    _resizeToContainer() {
      const container = this.canvas.parentElement || this.canvas;
      const rect = container.getBoundingClientRect();
      this.canvas.width = Math.max(1, Math.floor(rect.width));
      this.canvas.height = Math.max(1, Math.floor(rect.height));
    }

    _toScreen(camera) {
      return {
        x: camera.worldX * this.fitScaleX * this.scale + this.offsetX,
        y: camera.worldY * this.fitScaleY * this.scale + this.offsetY
      };
    }

    _visualScale() {
      return Math.min(this.fitScaleX, this.fitScaleY) * this.scale;
    }

    _zoomLevelFromScale() {
      if (this.scale < 0.4) return "site";
      if (this.scale < 0.9) return "wing";
      return "room";
    }

    _zoneFor(camera) {
      const { x, y } = this._toScreen(camera);
      const width = this.canvas.width;
      const height = this.canvas.height;
      const halfTileW = (TILE_WIDTH / 2) * this.fitScaleX * this.scale;
      const halfTileH = (TILE_HEIGHT / 2) * this.fitScaleY * this.scale;
      const within =
        x + halfTileW >= -PREWARM_MARGIN_PX &&
        x - halfTileW <= width + PREWARM_MARGIN_PX &&
        y + halfTileH >= -PREWARM_MARGIN_PX &&
        y - halfTileH <= height + PREWARM_MARGIN_PX;
      if (!within) return null;

      const centerX = width / 2;
      const centerY = height / 2;
      const minDim = Math.min(width, height);
      const distFromCenter = Math.hypot(x - centerX, y - centerY);
      if (distFromCenter <= minDim * FOCUS_RADIUS_FRACTION) return "focus";
      if (distFromCenter <= minDim * PERIPHERAL_RADIUS_FRACTION) return "peripheral";
      if (x + halfTileW < 0 || x - halfTileW > width || y + halfTileH < 0 || y - halfTileH > height) return "prewarm";
      return "peripheral";
    }

    _renderLoop() {
      this._render();
      this._raf = requestAnimationFrame(() => this._renderLoop());
    }

    _render() {
      const ctx = this.ctx;
      const width = this.canvas.width;
      const height = this.canvas.height;
      ctx.clearRect(0, 0, width, height);
      ctx.fillStyle = "#0b0d12";
      ctx.fillRect(0, 0, width, height);

      const zoomLevel = this.getZoomLevel();
      const msSincePanSettled = this._isSettled ? performance.now() - this._lastSettleAt : 0;

      const candidates = [];
      for (const camera of this.cameras) {
        const state = this.cameraState.get(camera.id);
        if (!state) continue;
        const zone = this._zoneFor(camera);
        if (zone === null) {
          this._applyTier(camera, state, "paused", 0);
          continue; // fully culled -- not even drawn, this is the bandwidth win
        }
        const { x, y } = this._toScreen(camera);
        const dist = Math.hypot(x - width / 2, y - height / 2);
        candidates.push({ camera, state, zone, dist });
      }

      // Bandwidth cap: an explicitly focused/tracked camera always keeps its
      // slot; remaining slots go to whichever visible tiles are closest to
      // the center of the screen. Anything beyond maxActiveStreams still
      // draws (so the grid structure stays intact) but is forced to
      // "paused" -- no stream opens for it.
      const prioritized = selectStreamingCandidates(candidates, this.maxActiveStreams);

      prioritized.forEach((entry) => {
        const proposedTier = entry.withinBudget
          ? resolveTierByZone({ zone: entry.zone, zoomLevel, isFocused: entry.state.isFocused, isTracking: entry.state.isTracking })
          : "paused";
        this._applyTier(entry.camera, entry.state, proposedTier, msSincePanSettled);
        this._drawTile(entry.camera, entry.state);
      });
    }

    _applyTier(camera, state, proposedTier, msSincePanSettled) {
      if (shouldApplyTierChange({ fromTier: state.tier, toTier: proposedTier, msSincePanSettled })) {
        state.tier = proposedTier;
        state.lastChangeAt = performance.now();
        this.onTierChange(camera, proposedTier);
      }
    }

    _drawTile(camera, state) {
      const { x, y } = this._toScreen(camera);
      const width = TILE_WIDTH * this.fitScaleX * this.scale;
      const height = TILE_HEIGHT * this.fitScaleY * this.scale;
      const visualScale = this._visualScale();
      const left = x - width / 2;
      const top = y - height / 2;
      const ctx = this.ctx;

      // Plain cell background -- matches the empty grey cells of the
      // reference VMS grid rather than a card/tile with its own chrome.
      ctx.fillStyle = "#3a3d42";
      ctx.fillRect(left, top, width, height);

      // Snapshot fills the whole cell if we have one; otherwise a faint
      // placeholder glyph, but only for tiles that are actually supposed to
      // be streaming -- a fully idle/paused tile stays blank, same as the
      // empty cells in the reference screenshot.
      const snapshot = this.snapshotCache.get(camera.id);
      if (snapshot && snapshot.complete && snapshot.naturalWidth) {
        ctx.save();
        ctx.beginPath();
        ctx.rect(left, top, width, height);
        ctx.clip();
        ctx.drawImage(snapshot, left, top, width, height);
        ctx.restore();
      } else if (state.tier !== "paused") {
        ctx.fillStyle = "#5a5f68";
        ctx.font = `${28 * visualScale}px Segoe UI, sans-serif`;
        ctx.textAlign = "center";
        ctx.textBaseline = "middle";
        ctx.fillText("\u25CE", x, y);
        ctx.textAlign = "left";
        ctx.textBaseline = "alphabetic";
      }

      // Border: thin grey normally, red/pink and thicker when this tile is
      // the current selection -- matches the reference screenshot's
      // selection highlight on an otherwise plain grid.
      ctx.lineWidth = state.isFocused ? 2 : 1;
      ctx.strokeStyle = state.isFocused ? "#e0526a" : "#54585f";
      ctx.strokeRect(left + ctx.lineWidth / 2, top + ctx.lineWidth / 2, width - ctx.lineWidth, height - ctx.lineWidth);

      // Small name label, top-left -- only once tiles are big enough on
      // screen to read; skipped when zoomed far out to avoid clutter.
      if (visualScale > 0.35) {
        ctx.font = `${11 * visualScale}px Segoe UI, sans-serif`;
        const label = camera.name || camera.id;
        const textWidth = ctx.measureText(label).width;
        ctx.fillStyle = "rgba(0,0,0,0.55)";
        ctx.fillRect(left, top, Math.min(width, textWidth + 12 * visualScale), 18 * visualScale);
        ctx.fillStyle = "#eef0f4";
        ctx.textBaseline = "middle";
        ctx.fillText(label, left + 5 * visualScale, top + 9 * visualScale);
        ctx.textBaseline = "alphabetic";
      }

      // Tiny tier-state dot, top-right corner -- replaces the old badge;
      // enough to see stream state at a glance without adding visual noise.
      ctx.beginPath();
      ctx.arc(left + width - 8 * visualScale, top + 8 * visualScale, 3.5 * visualScale, 0, Math.PI * 2);
      ctx.fillStyle = TIER_ACCENT[state.tier];
      ctx.fill();
    }

    _roundedRect(x, y, width, height, radius) {
      const ctx = this.ctx;
      ctx.beginPath();
      ctx.moveTo(x + radius, y);
      ctx.arcTo(x + width, y, x + width, y + height, radius);
      ctx.arcTo(x + width, y + height, x, y + height, radius);
      ctx.arcTo(x, y + height, x, y, radius);
      ctx.arcTo(x, y, x + width, y, radius);
      ctx.closePath();
    }
  }

  global.SpatialCanvas = SpatialCanvas;
  global.SpatialCanvasLayout = { layoutGrid, percentToWorld };
  global.SpatialCanvasPolicy = {
    TIER_RANK,
    resolveTierByZone,
    shouldApplyTierChange,
    selectStreamingCandidates
  };
})(window);
