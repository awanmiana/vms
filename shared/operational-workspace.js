(function operationalWorkspaceModule(root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) module.exports = api;
  else {
    root.VmsOperationalWorkspace = api;
    if (root.document) root.VmsOperationalWorkspaces = api.mountOperationalWorkspaces(root.document);
  }
})(typeof globalThis !== "undefined" ? globalThis : this, function createOperationalWorkspaceModule() {
  "use strict";

  const WORKSPACE_CONFIGS = Object.freeze({
    live: Object.freeze({
      kind: "live",
      rootId: "operatorView",
      tabsId: "liveWorkspaceTabs",
      panelId: "",
      canvasWrapId: "liveCanvasWrap",
      canvasId: "spatialCanvas",
      gridId: "simpleLiveGrid",
      searchId: "treeSearchInput",
      listId: "cameraList",
      countId: "cameraCount",
      resourcePaneId: "resourcePane",
      autoSwitchPaneId: "autoSwitchPane",
      resourceTabAttribute: "data-resource-tab",
      layoutPopoverId: "layoutPopover",
      divisionAttribute: "data-division",
      customDivisionInputId: "gridDivisionCustomInput",
      customDivisionButtonId: "gridDivisionCustomBtn",
      resourceToggleId: "liveResourceToggleBtn",
      inspectorToggleId: "liveInspectorToggleBtn",
      layoutButtonId: "gridLayoutBtn",
      closeButtonId: "gridCloseAllBtn",
      fullscreenButtonId: "gridFullscreenBtn",
      drawerId: "monitorDrawer",
      drawerTitleId: "monitorDrawerTitle",
      drawerStatusId: "monitorDrawerStatus",
      drawerBodyId: "monitorDrawerBody",
      maxDivision: 64
    }),
    playback: Object.freeze({
      kind: "playback",
      rootId: "playbackView",
      tabsId: "playbackWorkspaceTabs",
      panelId: "playbackPanel",
      canvasWrapId: "playbackCanvasWrap",
      canvasId: "playbackSpatialCanvas",
      gridId: "playbackTileGrid",
      searchId: "pbTreeSearchInput",
      listId: "pbCameraList",
      countId: "pbCameraCount",
      resourcePaneId: "pbResourcePane",
      autoSwitchPaneId: "pbAutoSwitchPane",
      resourceTabAttribute: "data-pb-resource-tab",
      layoutPopoverId: "pbLayoutPopover",
      divisionAttribute: "data-pb-division",
      customDivisionInputId: "pbDivisionCustomInput",
      customDivisionButtonId: "pbDivisionCustomBtn",
      resourceToggleId: "playbackResourceToggleBtn",
      inspectorToggleId: "playbackInspectorToggleBtn",
      layoutButtonId: "pbLayoutBtn",
      closeButtonId: "pbCloseAllBtn",
      fullscreenButtonId: "pbFullscreenBtn",
      drawerId: "playbackDrawer",
      drawerTitleId: "playbackDrawerTitle",
      drawerStatusId: "playbackDrawerStatus",
      drawerBodyId: "playbackDrawerBody",
      maxDivision: 16
    })
  });

  function divisionButtons(config) {
    return [1, 4, 9, 16, 25, 36, 49, 64]
      .filter((division) => division <= config.maxDivision)
      .map((division) => {
        const side = Math.sqrt(division);
        return `<button ${config.divisionAttribute}="${division}" type="button">${side} x ${side}</button>`;
      })
      .join("");
  }

  function renderResourcePanel(config) {
    const playback = config.kind === "playback";
    return `
      <aside class="camera-browser resource-panel video-overlay-panel video-resource-panel is-overlay-open" data-workspace-part="resource">
        <div class="resource-tabs">
          <button class="resource-tab active" ${config.resourceTabAttribute}="resource" type="button">Resource</button>
          <button class="resource-tab" ${config.resourceTabAttribute}="autoswitch" type="button">Auto-Switch</button>
        </div>
        <div class="resource-search"><input id="${config.searchId}" type="search" placeholder="Search" autocomplete="off" /></div>
        <div id="${config.resourcePaneId}" class="resource-pane"><div id="${config.listId}" class="camera-list camera-tree"></div></div>
        <div id="${config.autoSwitchPaneId}" class="resource-pane" hidden>
          <div class="empty-state">Auto-Switch (${playback ? "playback sequence" : "camera tour"}/cycling) is not built yet.</div>
        </div>
        <div class="tree-footer"><span id="${config.countId}"></span></div>
      </aside>`;
  }

  function renderLiveToolbarExtensions() {
    return `
      <button id="gridZoomOutBtn" class="grid-tool-btn" title="Digital zoom out" type="button">-</button>
      <button id="gridZoomInBtn" class="grid-tool-btn" title="Digital zoom in" type="button">+</button>
      <div class="grid-tool-popover ptz-tool-popover" id="ptzPopover" hidden>
        <div class="ptz-popover-title">PTZ</div>
        <div class="ptz-pad compact">
          ${Object.entries({
            "up-left": "&#8598;", up: "&#8593;", "up-right": "&#8599;",
            left: "&#8592;", stop: "Stop", right: "&#8594;",
            "down-left": "&#8601;", down: "&#8595;", "down-right": "&#8600;"
          }).map(([action, label]) => `<button data-ptz-action="${action}" type="button">${label}</button>`).join("")}
        </div>
        <div class="ptz-popover-actions"><button data-ptz-action="zoom-in" type="button">Zoom +</button><button data-ptz-action="zoom-out" type="button">Zoom -</button></div>
        <p id="ptzStatus" class="grid-tool-hint">PTZ is unavailable until a verified device adapter operation is implemented.</p>
      </div>
      <button id="gridPtzBtn" class="grid-tool-btn" title="PTZ controls" type="button">PTZ</button>`;
  }

  function renderLiveSettingsExtension() {
    return `
      <span class="grid-tool-sep"></span>
      <div class="grid-tool-popover" id="streamSettingsPopover" hidden>
        <label><span>Total camera sessions</span><input id="maxActiveStreamsInputGrid" type="number" min="1" max="64" /></label>
        <p class="grid-tool-hint">Shared by every Live View and Remote Playback workspace. Automatic mode is managed in Settings.</p>
      </div>
      <button id="gridSettingsBtn" class="grid-tool-btn" title="Streaming settings" type="button">&#9881;</button>`;
  }

  function renderToolbar(config) {
    const live = config.kind === "live";
    return `
      <div class="grid-floating-toolbar" data-workspace-part="toolbar">
        <div class="grid-tool-popover" id="${config.layoutPopoverId}" hidden>
          ${divisionButtons(config)}
          <div class="grid-tool-custom">
            <input id="${config.customDivisionInputId}" type="number" min="1" max="${config.maxDivision}" placeholder="Custom (1-${config.maxDivision})" />
            <button id="${config.customDivisionButtonId}" type="button">Set</button>
          </div>
        </div>
        <button id="${config.resourceToggleId}" class="grid-tool-btn panel-toggle-btn" title="Show or hide cameras and groups" type="button">Cams</button>
        <button id="${config.inspectorToggleId}" class="grid-tool-btn panel-toggle-btn" title="Show or hide selected camera information and controls" type="button">Info</button>
        <button id="${config.layoutButtonId}" class="grid-tool-btn" title="Window division" type="button">&#9638;</button>
        ${live ? renderLiveToolbarExtensions() : ""}
        <button id="${config.closeButtonId}" class="grid-tool-btn" title="Close all" type="button">&#10005;</button>
        <button id="${config.fullscreenButtonId}" class="grid-tool-btn" title="Fullscreen" type="button">&#9974;</button>
        ${live ? renderLiveSettingsExtension() : ""}
      </div>`;
  }

  function renderPlaybackExtension() {
    return `
      <div class="pb-scrubber" id="pbScrubber" data-workspace-extension="playback">
        <div class="pb-transport">
          <button id="pbJumpStartBtn" class="grid-tool-btn" title="Jump to start of day" type="button">&#9198;</button>
          <button id="pbStepBackBtn" class="grid-tool-btn" title="Step back 10s" type="button">&#9664;</button>
          <button id="pbPlayBtn" class="grid-tool-btn" title="Play / pause" type="button">&#9654;</button>
          <button id="pbStepForwardBtn" class="grid-tool-btn" title="Step forward 10s" type="button">&#9654;&#9654;</button>
          <button id="pbJumpEndBtn" class="grid-tool-btn" title="Jump to end of day" type="button">&#9197;</button>
          <span id="pbCurrentTime" class="pb-time-pill">00:00:00</span>
          <button id="pbSpeedBtn" class="secondary pb-speed-btn" type="button">1x</button>
          <div class="pb-range-control">
            <button id="pbRangeBtn" class="secondary pb-range-btn" type="button"><span class="pb-range-icon" aria-hidden="true">&#128197;</span><span id="pbRangeLabel">Time range</span></button>
            <div class="grid-tool-popover pb-range-popover" id="pbRangePopover" hidden>
              <label><span>Start</span><div class="pb-range-inputs"><input id="pbRangeStartDate" type="date" /><input id="pbRangeStartTime" type="time" step="1" value="00:00:00" /></div></label>
              <label><span>End</span><div class="pb-range-inputs"><input id="pbRangeEndDate" type="date" /><input id="pbRangeEndTime" type="time" step="1" value="23:59:59" /></div></label>
              <div class="form-actions"><button id="pbRangeApplyBtn" type="button">Search footage</button></div>
            </div>
          </div>
          <span class="pb-transport-spacer"></span>
          <span class="pb-footage-legend" title="Prototype footage availability for the selected search window"><span><i class="available"></i>Available</span><span><i class="missing"></i>Missing</span></span>
          <label class="pb-filter-chip"><input id="pbFilterHuman" type="checkbox" /> Human</label>
          <label class="pb-filter-chip"><input id="pbFilterVehicle" type="checkbox" /> Vehicle</label>
        </div>
        <div class="pb-track-wrap">
          <div class="pb-track" id="pbTrack" role="slider" tabindex="0" aria-label="Playback timeline" aria-valuemin="0" aria-valuemax="86399" aria-valuenow="0">
            <div class="pb-track-available" id="pbTrackAvailable"></div><div class="pb-ticks" id="pbTicks"></div><div class="pb-playhead" id="pbPlayhead"></div>
          </div>
          <div class="pb-date-chip"><button id="pbPrevDayBtn" type="button" title="Previous day">&#8249;</button><span id="pbDateLabel">--/--</span><button id="pbNextDayBtn" type="button" title="Next day">&#8250;</button></div>
        </div>
      </div>`;
  }

  function renderWorkspacePanel(config) {
    const playback = config.kind === "playback";
    return `
      <section class="live-panel ${playback ? "playback-panel " : ""}video-workspace-panel" ${config.panelId ? `id="${config.panelId}"` : ""} data-workspace-part="stage">
        <div id="${config.tabsId}" class="workspace-tabbar" data-workspace-kind="${config.kind}"></div>
        <div class="live-canvas-wrap ${playback ? "playback-canvas-wrap" : ""}" id="${config.canvasWrapId}">
          <canvas id="${config.canvasId}"></canvas>
          <div id="${config.gridId}" class="simple-live-grid ${playback ? "playback-tile-grid" : ""}"></div>
          ${renderToolbar(config)}
        </div>
        ${playback ? renderPlaybackExtension() : ""}
      </section>`;
  }

  function renderDrawer(config) {
    return `
      <aside class="monitor-drawer video-overlay-panel video-inspector-panel is-overlay-open" id="${config.drawerId}" data-workspace-part="inspector">
        <div class="drawer-header"><div><div class="drawer-kicker">Selected Camera</div><h3 id="${config.drawerTitleId}">No camera selected</h3></div><span id="${config.drawerStatusId}" class="status idle">idle</span></div>
        <div id="${config.drawerBodyId}" class="drawer-body"></div>
      </aside>`;
  }

  function renderOperationalWorkspace(kind) {
    const config = WORKSPACE_CONFIGS[kind];
    if (!config) throw new Error(`Unknown operational workspace kind: ${kind}.`);
    return `${renderResourcePanel(config)}${renderWorkspacePanel(config)}${renderDrawer(config)}`;
  }

  class OperationalWorkspaceComponent {
    constructor(rootElement, config) {
      this.root = rootElement;
      this.config = config;
      this.root.dataset.operationalWorkspace = config.kind;
      this.root.innerHTML = renderOperationalWorkspace(config.kind);
    }

    part(name) {
      return this.root.querySelector(`[data-workspace-part="${name}"]`);
    }

    setOverlay(name, open) {
      const panel = this.part(name);
      panel?.classList.toggle("is-overlay-open", Boolean(open));
      return Boolean(open);
    }

    toggleOverlay(name) {
      const panel = this.part(name);
      return this.setOverlay(name, !panel?.classList.contains("is-overlay-open"));
    }

    async toggleFullscreen(documentRef = this.root.ownerDocument) {
      if (documentRef.fullscreenElement) return documentRef.exitFullscreen();
      if (typeof this.root.requestFullscreen === "function") return this.root.requestFullscreen();
      return undefined;
    }
  }

  function mountOperationalWorkspaces(documentRef) {
    const instances = {};
    Object.values(WORKSPACE_CONFIGS).forEach((config) => {
      const element = documentRef.getElementById(config.rootId);
      if (element) instances[config.kind] = new OperationalWorkspaceComponent(element, config);
    });
    return Object.freeze(instances);
  }

  return Object.freeze({
    OperationalWorkspaceComponent,
    WORKSPACE_CONFIGS,
    mountOperationalWorkspaces,
    renderOperationalWorkspace
  });
});
