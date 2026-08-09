// P3-14 slice 1 — the governed grid, drawn in honest per-tile state.
//
// Each tile's border and status strip carry its real state so a non-live tile
// is never shown as if it were live: green = live at the requested tier, amber
// = degraded (decoding below what was asked), red = capacity-paused (wanted
// video but the machine's budget forced it off), slate = intentionally
// off-screen. The focused/working-set tile carries a white ring. The header is
// the aggregate capacity meter. Everything is bound to the C++ `governor`
// controller, which re-plans on each focus sweep.

import QtQuick
import QtQuick.Window
import Vms 1.0

Window {
    id: root
    visible: true
    width: 1280
    height: 800
    title: "VMS Native — Workspace (P3-14 · honest per-tile state)"
    color: "#0e1014"

    // Two independently stateful instances of one workspace (P3-14): Live and
    // Playback. `governor` points at whichever tab is active, so the entire view
    // below re-binds to that instance's state with no duplication.
    property int tabIndex: 0
    property var governor: root.tabIndex === 0 ? liveCtrl : playbackCtrl

    // Workspace UI state (P3-14 layered panels).
    property bool showBrowser: true
    property bool showSiteOps: true
    property string cameraSearch: ""
    // Spatial canvas mode (inc 24, P3-15/P3-01): the Live grid becomes a
    // pannable/zoomable canvas of draggable camera tiles; media cost follows
    // the viewport. --spatial starts in this mode.
    property bool spatialMode: (typeof spatialDefault !== "undefined")
                               ? spatialDefault : false

    // Command palette (inc 25, P1-12/A0): every operator verb as one validated
    // text command through the envelope (Ctrl+K or the ⌘ toolbar button).
    property bool showPalette: false
    // Alarm surface (inc 27, P6-03/P6-07).
    property bool showAlarms: false

    // EVERY state-changing UI action goes through the command envelope
    // (A0 / P1-13): one validated, capability-checked, confirm-gated, audited
    // gate for clicks, palette lines, and the external API alike. There is
    // deliberately NO direct-controller fallback — a bypass would be an action
    // with no audit row, and --coverage-check fails the build on one.
    // Returns { ok, outcome, message }.
    function cmd(id, args, confirm) {
        if (typeof commander === "undefined" || !commander)
            return { ok: false, outcome: "unavailable",
                     message: "command envelope unavailable" }
        return commander.invoke(id, args || ({}), confirm === true)
    }
    property bool commandsAvailable: (typeof commander !== "undefined")
                                     && commander !== null

    function alarmAction(verb, id) {
        root.cmd("alarm." + verb, { "id": id })
    }

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: root.showPalette = !root.showPalette
    }

    // The envelope owns actions; UI state (like the spatial mode) stays in QML,
    // so the workspace.spatial command round-trips through this signal.
    Connections {
        target: (typeof commander !== "undefined") ? commander : null
        // UI state follows the command (the command itself stops the sweep).
        function onSpatialModeRequested(on) { root.spatialMode = on }
    }

    // The prototype's spatial tier labels (Idle / Preview / Live-SD / Live-HD).
    function spatialTierLabel(t) {
        switch (t) {
        case "MAIN":  return "Live · HD";
        case "SUB":   return "Live · SD";
        case "THUMB": return "Preview";
        }
        return "Idle";
    }

    function stateColor(s) {
        switch (s) {
        case "live":             return "#37c871";
        case "degraded":         return "#f2a33c";
        case "paused-capacity":  return "#e05a4e";
        case "paused-offscreen": return "#5b6472";
        }
        return "#8a93a3";
    }

    // Filter the tile model for the resource browser by "cam N", tier, or state.
    function filterTiles(tiles, q) {
        if (!q || q.length === 0) return tiles;
        var ql = q.toLowerCase();
        var out = [];
        for (var i = 0; i < tiles.length; i++) {
            var t = tiles[i];
            if (("cam " + t.id).toLowerCase().indexOf(ql) !== -1
                || String(t.tier).toLowerCase().indexOf(ql) !== -1
                || String(t.state).toLowerCase().indexOf(ql) !== -1)
                out.push(t);
        }
        return out;
    }

    Column {
        anchors.fill: parent
        spacing: 0

        // --- Live / Playback tabs (two independently stateful instances) ---
        Rectangle {
            id: tabBar
            width: parent.width
            height: 36
            color: "#0b0d11"

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                Repeater {
                    model: [ { label: "Live", idx: 0 },
                             { label: "Playback", idx: 1 },
                             { label: "Devices", idx: 2 } ]
                    delegate: Rectangle {
                        property bool active: root.tabIndex === modelData.idx
                        width: tabText.width + 30; height: 26; radius: 6
                        color: active ? "#1b2230" : "transparent"
                        border.color: active ? "#3a6ea5" : "transparent"
                        border.width: 1
                        Text {
                            id: tabText; anchors.centerIn: parent
                            text: modelData.label
                            color: active ? "#e8ecf3" : "#8a93a3"
                            font.pixelSize: 13; font.bold: active
                        }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: root.tabIndex = modelData.idx
                        }
                    }
                }
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: root.tabIndex === 0
                      ? "live view"
                      : root.tabIndex === 2
                        ? ((typeof devicesCtrl !== "undefined" && devicesCtrl)
                           ? ("device management — " + devicesCtrl.deviceCount + " device(s)")
                           : "device management — persistence unavailable")
                        : ((typeof playback !== "undefined" && playback)
                           ? ("playback — " + playback.spans.length + " footage span(s)")
                           : "playback — no recording index")
                color: "#5a6270"; font.pixelSize: 11
            }
        }

        // --- header / capacity meter + toolbar ---
        // Hidden on the Devices tab (tabIndex 2), which is not a governed view.
        Rectangle {
            id: header
            visible: root.tabIndex !== 2
            width: parent.width
            height: 92
            color: "#141821"

            // Toolbar (top-right): toggle the camera browser and the auto sweep.
            Row {
                id: toolbar
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 20
                anchors.topMargin: 12
                spacing: 8

                Text {
                    text: "layout"
                    color: "#6f7a86"; font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                }
                Repeater {
                    model: [4, 9, 16, 25, 64]
                    delegate: Rectangle {
                        property bool active: governor.tiles.length === modelData
                        width: 32; height: 28; radius: 6
                        color: active ? "#2a3446" : "#1a1f28"
                        border.color: active ? "#3a6ea5" : "#333c4c"; border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            color: active ? "#e8ecf3" : "#9aa4b4"; font.pixelSize: 12
                        }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: root.cmd("workspace.layout",
                                                { "count": modelData })
                        }
                    }
                }

                Rectangle { width: 1; height: 24; color: "#2a3240"
                    anchors.verticalCenter: parent.verticalCenter }

                // Spatial canvas toggle (inc 24) — Live tab only. Entering
                // spatial mode stops the auto sweep: the viewport, not a sweep,
                // owns the working set there.
                Rectangle {
                    visible: root.tabIndex === 0
                    width: spatialBtnText.width + 22; height: 28; radius: 6
                    color: root.spatialMode ? "#2a3446" : "#1a1f28"
                    border.color: root.spatialMode ? "#3a6ea5" : "#333c4c"
                    border.width: 1
                    Text {
                        id: spatialBtnText; anchors.centerIn: parent
                        text: root.spatialMode ? "⊞ Grid" : "⌖ Spatial"
                        color: "#cbd3df"; font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        // The command flips the mode (via spatialModeRequested)
                        // and stops the sweep; QML never touches the controller.
                        onClicked: root.cmd("workspace.spatial",
                                            { "on": !root.spatialMode })
                    }
                }

                Rectangle {
                    width: camBtnText.width + 22; height: 28; radius: 6
                    color: root.showBrowser ? "#2a3446" : "#1a1f28"
                    border.color: "#333c4c"; border.width: 1
                    Text {
                        id: camBtnText; anchors.centerIn: parent
                        text: "☰ Cameras"; color: "#cbd3df"; font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.showBrowser = !root.showBrowser
                    }
                }
                Rectangle {
                    visible: root.tabIndex === 0
                             && (typeof siteOps !== "undefined") && siteOps
                    width: siteOpsBtnText.width + 22; height: 28; radius: 6
                    color: root.showSiteOps ? "#2a3446" : "#1a1f28"
                    border.color: root.showSiteOps ? "#3a6ea5" : "#333c4c"
                    border.width: 1
                    Text {
                        id: siteOpsBtnText; anchors.centerIn: parent
                        text: "Site Ops"; color: "#cbd3df"; font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.showSiteOps = !root.showSiteOps
                    }
                }
                // Alarms chip (inc 27): honest attention count, red when
                // something unacknowledged demands an operator.
                Rectangle {
                    property int attn: (typeof alarmsCtrl !== "undefined" && alarmsCtrl)
                                       ? alarmsCtrl.needsAttention : 0
                    visible: (typeof alarmsCtrl !== "undefined") && alarmsCtrl !== null
                    width: alarmBtnText.width + 22; height: 28; radius: 6
                    color: attn > 0 ? "#4a1f1f" : (root.showAlarms ? "#2a3446" : "#1a1f28")
                    border.color: attn > 0 ? "#e05a4e"
                                           : (root.showAlarms ? "#3a6ea5" : "#333c4c")
                    border.width: 1
                    Text {
                        id: alarmBtnText; anchors.centerIn: parent
                        text: "🔔 Alarms" + (parent.attn > 0 ? " (" + parent.attn + ")" : "")
                        color: parent.attn > 0 ? "#f0b8b0" : "#cbd3df"
                        font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.showAlarms = !root.showAlarms
                    }
                }
                // Command palette (inc 25): one validated gate for every verb.
                Rectangle {
                    visible: (typeof commander !== "undefined") && commander !== null
                    width: cmdBtnText.width + 22; height: 28; radius: 6
                    color: root.showPalette ? "#2a3446" : "#1a1f28"
                    border.color: root.showPalette ? "#3a6ea5" : "#333c4c"
                    border.width: 1
                    Text {
                        id: cmdBtnText; anchors.centerIn: parent
                        text: "⌘ Command"; color: "#cbd3df"; font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.showPalette = !root.showPalette
                    }
                }
                Rectangle {
                    width: autoBtnText.width + 22; height: 28; radius: 6
                    color: governor.autoSweeping ? "#243a2c" : "#1a1f28"
                    border.color: "#333c4c"; border.width: 1
                    Text {
                        id: autoBtnText; anchors.centerIn: parent
                        text: governor.autoSweeping ? "⏸ Auto sweep" : "▶ Auto sweep"
                        color: "#cbd3df"; font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.cmd("workspace.sweep",
                                            { "on": !governor.autoSweeping })
                    }
                }
            }

            Column {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 20
                spacing: 4

                Text {
                    text: "Governed workspace — " + governor.profileLabel
                    color: "#e8ecf3"
                    font.pixelSize: 18
                    font.bold: true
                }
                Text {
                    text: governor.capacity
                    color: "#9aa4b4"
                    font.pixelSize: 14
                }
                Text {
                    text: governor.overflow
                          ? "⚠ over capacity: some visible tiles are capacity-paused"
                          : (root.spatialMode && root.tabIndex === 0
                             ? "spatial canvas · " + governor.zoomLevelName(spatialView.zoom)
                               + " level · wheel zooms, drag a tile to place it, drag space to pan"
                             : "focus on tile " + governor.focusIndex
                               + "  ·  " + governor.columns + "×" + governor.rows + " grid")
                    color: governor.overflow ? "#e05a4e" : "#6f7a86"
                    font.pixelSize: 13
                }
                // Live optimizer read-out (inc 8): what the health sampler adjusted
                // and why. Empty (hidden) until the sampler runs.
                Text {
                    visible: governor.optimizer.length > 0
                    text: governor.optimizer
                    color: "#5aa0e0"
                    font.pixelSize: 12
                }
            }

            // Legend (bottom-right, under the toolbar)
            Row {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 20
                anchors.bottomMargin: 12
                spacing: 16

                Repeater {
                    model: [
                        { c: "live",             t: "live" },
                        { c: "degraded",         t: "degraded" },
                        { c: "paused-capacity",  t: "capacity-paused" },
                        { c: "paused-offscreen", t: "off-screen" }
                    ]
                    delegate: Row {
                        spacing: 6
                        Rectangle {
                            width: 12; height: 12; radius: 3
                            anchors.verticalCenter: parent.verticalCenter
                            color: root.stateColor(modelData.c)
                        }
                        Text {
                            text: modelData.t
                            color: "#9aa4b4"
                            font.pixelSize: 12
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }

        // --- the governed grid ---
        Item {
            id: gridArea
            visible: root.tabIndex !== 2
            width: parent.width
            height: parent.height - header.height - tabBar.height

            // No gaps over live video, so each chrome cell sits exactly on its
            // composited cell (both split this rectangle into rows x cols equal
            // fractions); a small gap in the state-only mode.
            property int spacingPx: videoActive ? 0 : 8
            property int cols: Math.max(1, governor.columns)
            property int rowsN: Math.max(1, governor.rows)
            property real cellW: (width  - spacingPx * (cols  + 1)) / cols
            property real cellH: (height - spacingPx * (rowsN + 1)) / rowsN

            // Live governed video, drawn behind the chrome and filling exactly
            // the grid area so it aligns with the cells. Fed from C++ (a
            // GStreamer appsink) only under --video; hidden otherwise.
            VideoItem {
                id: videoLayer
                objectName: "videoOut"
                anchors.fill: parent
                // The pipeline follows the Live instance; on the Playback tab
                // there is no recorded footage yet, so the video is hidden.
                // Hidden as a full composite on the spatial canvas. Room-level
                // tiles subscribe to and crop this same frame below; no second
                // decode/compositor session is opened.
                visible: videoActive && root.tabIndex === 0 && !root.spatialMode
            }

            // Recorded video for the Playback tab (inc 7c-3): a second VideoItem
            // fed by a PlaybackPipeline decoding the recorded .mp4 at the playhead.
            // Placed above the (opaque) playback tile chrome but below the panels
            // and transport, and only shown on the Playback tab.
            VideoItem {
                id: playbackVideoLayer
                objectName: "playbackVideoOut"
                anchors.fill: parent
                z: 1
                visible: root.tabIndex === 1
                       && (typeof playback !== "undefined") && playback !== null
            }

            Grid {
                // The fixed grid yields to the spatial canvas on the Live tab
                // (inc 24); Playback keeps the classic grid.
                visible: !(root.spatialMode && root.tabIndex === 0)
                anchors.fill: parent
                anchors.margins: gridArea.spacingPx
                columns: gridArea.cols
                spacing: gridArea.spacingPx

                Repeater {
                    model: governor.tiles

                    delegate: Rectangle {
                        width: gridArea.cellW
                        height: gridArea.cellH
                        radius: 6
                        // Translucent over live video so the picture shows
                        // through the chrome; opaque in the state-only mode and on
                        // the (footage-less) Playback tab.
                        color: (videoActive && root.tabIndex === 0)
                               ? Qt.rgba(0.055, 0.063, 0.078, 0.32)
                               : "#171a21"
                        border.color: root.stateColor(modelData.state)
                        border.width: modelData.focused ? 3 : 1.5

                        // tile id, top-left
                        Text {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.margins: 8
                            text: "#" + modelData.id
                            color: "#6b7482"
                            font.pixelSize: 12
                        }

                        // focus marker, top-right
                        Text {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 8
                            visible: modelData.focused
                            text: "◉ focus"
                            color: "#e8ecf3"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        // high-priority pin, top-left under the id (persists even
                        // when this tile is not the focused one)
                        Text {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.leftMargin: 8
                            anchors.topMargin: 26
                            visible: modelData.priority === "high"
                            text: "★ HIGH"
                            color: "#f2c94c"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        // tier + state, centered
                        Column {
                            anchors.centerIn: parent
                            spacing: 4
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.tier
                                color: "#e8ecf3"
                                font.pixelSize: Math.max(14, Math.min(34, gridArea.cellH * 0.22))
                                font.bold: true
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.stateText
                                color: root.stateColor(modelData.state)
                                font.pixelSize: Math.max(11, Math.min(18, gridArea.cellH * 0.11))
                            }
                        }

                        // status strip along the bottom
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: parent.border.width
                            height: 5
                            radius: 2
                            color: root.stateColor(modelData.state)
                        }

                        // Operator picks the working set: click to focus this
                        // tile. The governor re-plans (and, under --video, the
                        // live picture follows) so the focused tile is protected
                        // at Main and the rest degrade around it.
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.cmd("workspace.focus",
                                                { "tile": modelData.id })
                        }
                    }
                }
            }

            // Spatial camera canvas (inc 24, P3-15/P3-01): a pannable, zoomable
            // world of draggable camera tiles. The viewport drives the governor
            // (WorkspaceController::updateSpatialViewport, the prototype-exact
            // policy): tiles near center earn Main, peripheral ones Thumb,
            // culled ones truly stop decoding. Wheel zooms toward the cursor;
            // dragging empty space pans; dragging a tile repositions it
            // (persisted); a click focuses, exactly like the grid.
            Item {
                id: spatialView
                objectName: "spatialView"
                visible: root.spatialMode && root.tabIndex === 0
                anchors.fill: parent
                clip: true

                property real zoom: 1.0
                property real offX: 0
                property real offY: 0

                // The 300ms promote-dwell (SpatialPolicy): interactions apply
                // downgrades at once (settled=false); the timer firing applies
                // the held promotions (settled=true).
                function markUnsettled() {
                    settleTimer.restart()
                    governor.updateSpatialViewport(width, height, zoom,
                                                   offX, offY, false)
                }
                Timer {
                    id: settleTimer
                    interval: 300
                    onTriggered: governor.updateSpatialViewport(
                                     spatialView.width, spatialView.height,
                                     spatialView.zoom, spatialView.offX,
                                     spatialView.offY, true)
                }

                function fitToView() {
                    var tiles = governor.tiles
                    if (!tiles.length || width <= 0 || height <= 0) return
                    var minX = 1e12, minY = 1e12, maxX = -1e12, maxY = -1e12
                    for (var i = 0; i < tiles.length; i++) {
                        minX = Math.min(minX, tiles[i].px - 160)
                        maxX = Math.max(maxX, tiles[i].px + 160)
                        minY = Math.min(minY, tiles[i].py - 95)
                        maxY = Math.max(maxY, tiles[i].py + 95)
                    }
                    var z = Math.min(width / Math.max(1, maxX - minX),
                                     height / Math.max(1, maxY - minY)) * 0.92
                    zoom = Math.max(0.15, Math.min(6, z))
                    offX = (width - (minX + maxX) * zoom) / 2
                    offY = (height - (minY + maxY) * zoom) / 2
                    markUnsettled()
                }
                function videoStats() {
                    var consumers = 0, receiving = 0, delivered = 0
                    var eligible = 0
                    var unique = 0, ids = ({})
                    var cropsValid = true
                    for (var i = 0; i < spatialTileRepeater.count; i++) {
                        var tile = spatialTileRepeater.itemAt(i)
                        if (tile && (tile.honestState === "live"
                                     || tile.honestState === "degraded"))
                            eligible++
                        var consumer = tile ? tile.videoConsumer : null
                        if (!consumer) continue
                        consumers++
                        if (consumer.frameCount > 0) receiving++
                        delivered += consumer.frameCount
                        var id = consumer.sourceIndex
                        if (id < 0 || id >= consumer.sourceColumns * consumer.sourceRows)
                            cropsValid = false
                        if (!ids[id]) { ids[id] = true; unique++ }
                    }
                    return { "delegates": spatialTileRepeater.count,
                             "eligible": eligible,
                             "consumers": consumers, "receiving": receiving,
                             "delivered": delivered, "unique": unique,
                             "cropsValid": cropsValid }
                }
                onVisibleChanged: if (visible) fitToView()
                onWidthChanged: if (visible) markUnsettled()
                onHeightChanged: if (visible) markUnsettled()

                // Background: pan by dragging empty space; wheel zooms toward
                // the cursor (0.15–6×), exactly the prototype's interaction.
                MouseArea {
                    anchors.fill: parent
                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    property real lastX: 0
                    property real lastY: 0
                    onPressed: (mouse) => { lastX = mouse.x; lastY = mouse.y }
                    onPositionChanged: (mouse) => {
                        if (!pressed) return
                        spatialView.offX += mouse.x - lastX
                        spatialView.offY += mouse.y - lastY
                        lastX = mouse.x; lastY = mouse.y
                        spatialView.markUnsettled()
                    }
                    onWheel: (wheel) => {
                        var f = wheel.angleDelta.y > 0 ? 1.1 : 0.9
                        var nz = Math.max(0.15, Math.min(6, spatialView.zoom * f))
                        f = nz / spatialView.zoom
                        if (f === 1) return
                        spatialView.offX = wheel.x - (wheel.x - spatialView.offX) * f
                        spatialView.offY = wheel.y - (wheel.y - spatialView.offY) * f
                        spatialView.zoom = nz
                        spatialView.markUnsettled()
                    }
                }

                // inc 31: the active floor is real persisted premises metadata,
                // not a decorative hard-coded backdrop. The URI may be empty;
                // in that case the named blank floor is shown honestly.
                Item {
                    id: floorUnderlay
                    z: -2
                    x: spatialView.offX
                    y: spatialView.offY
                    width: ((typeof premises !== "undefined") && premises
                            ? premises.worldWidth : 1600) * spatialView.zoom
                    height: ((typeof premises !== "undefined") && premises
                             ? premises.worldHeight : 900) * spatialView.zoom

                    Rectangle {
                        anchors.fill: parent
                        color: "#111722"
                        border.color: "#354155"
                        border.width: Math.max(1, spatialView.zoom)
                    }
                    Image {
                        anchors.fill: parent
                        source: ((typeof premises !== "undefined") && premises)
                                ? premises.planUri : ""
                        visible: source.toString().length > 0
                        fillMode: Image.Stretch
                        asynchronous: true
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: !((typeof premises !== "undefined") && premises
                                   && premises.planUri.length > 0)
                        text: ((typeof premises !== "undefined") && premises)
                              ? premises.floorName + " · no plan image"
                              : "No premises store"
                        color: "#536078"
                        font.pixelSize: Math.max(11, 18 * spatialView.zoom)
                    }
                }

                Repeater {
                    id: spatialTileRepeater
                    // Bound only while visible so the hidden canvas costs nothing.
                    model: spatialView.visible ? governor.tiles : []
                    delegate: Rectangle {
                        id: sTile
                        property var videoConsumer: spatialVideoLoader.item
                        property string honestState: modelData.state
                        width: 320 * spatialView.zoom
                        height: 190 * spatialView.zoom
                        x: modelData.px * spatialView.zoom + spatialView.offX - width / 2
                        y: modelData.py * spatialView.zoom + spatialView.offY - height / 2
                        radius: 6 * Math.min(1, spatialView.zoom * 2)
                        color: "#171a21"
                        border.color: root.stateColor(modelData.state)
                        border.width: modelData.focused ? 3 : 1.5

                        // inc 33: progressive map media. Site stays pins, wing
                        // stays state cards, and room zoom reuses the one governed
                        // composite frame. Loader means non-live/offscreen/zoomed-
                        // out tiles have no texture consumer at all.
                        Loader {
                            id: spatialVideoLoader
                            objectName: "spatialVideoLoader"
                            anchors.fill: parent
                            anchors.margins: sTile.border.width
                            active: videoActive && spatialView.zoom >= 0.9
                                    && (modelData.state === "live"
                                        || modelData.state === "degraded")
                            sourceComponent: VideoItem {
                                objectName: "spatialVideoOut"
                                anchors.fill: parent
                                frameSource: videoLayer
                                sourceIndex: modelData.id
                                sourceColumns: gridArea.cols
                                sourceRows: gridArea.rowsN
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: sTile.border.width
                            visible: spatialVideoLoader.active
                            color: Qt.rgba(0.04, 0.05, 0.07, 0.18)
                        }

                        // Camera coverage is metadata only: this wedge never
                        // changes viewport tiering or implies decoded video.
                        Canvas {
                            id: fovWedge
                            z: -1
                            anchors.centerIn: parent
                            width: 520 * spatialView.zoom
                            height: 520 * spatialView.zoom
                            visible: spatialView.zoom > 0.28
                                     && modelData.state !== "paused-offscreen"
                            rotation: modelData.facing
                            property real aperture: modelData.fov
                            onApertureChanged: requestPaint()
                            onWidthChanged: requestPaint()
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                var cx = width / 2, cy = height / 2
                                var r = Math.min(width, height) * 0.48
                                var half = aperture * Math.PI / 360
                                ctx.beginPath()
                                ctx.moveTo(cx, cy)
                                ctx.arc(cx, cy, r, -half, half, false)
                                ctx.closePath()
                                ctx.fillStyle = modelData.focused
                                                ? "rgba(82,168,255,0.20)"
                                                : "rgba(82,168,255,0.10)"
                                ctx.fill()
                                ctx.strokeStyle = "rgba(82,168,255,0.55)"
                                ctx.lineWidth = Math.max(1, spatialView.zoom)
                                ctx.stroke()
                            }
                        }

                        // Honest chrome, scaled with the zoom: camera id, the
                        // prototype's spatial tier label, and the true state.
                        Column {
                            anchors.centerIn: parent
                            spacing: 2
                            visible: spatialView.zoom > 0.28
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Cam " + modelData.id
                                      + (modelData.priority === "high" ? " ★" : "")
                                color: "#e8ecf3"
                                font.pixelSize: Math.max(9, 15 * spatialView.zoom)
                                font.bold: modelData.focused
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: root.spatialTierLabel(modelData.tier)
                                color: root.stateColor(modelData.state)
                                font.pixelSize: Math.max(8, 12 * spatialView.zoom)
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: spatialView.zoom > 0.5
                                text: modelData.stateText
                                color: "#8a93a3"
                                font.pixelSize: Math.max(8, 10 * spatialView.zoom)
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: spatialView.zoom > 0.5
                                text: Math.round(modelData.facing) + "° · "
                                      + Math.round(modelData.fov) + "° FOV"
                                color: "#6daee8"
                                font.pixelSize: Math.max(8, 9 * spatialView.zoom)
                            }
                        }
                        // Zoomed far out (site level) a tile is just a dot-like
                        // pin — the prototype's "map pin" reading.
                        Rectangle {
                            anchors.centerIn: parent
                            visible: spatialView.zoom <= 0.28
                            width: 8; height: 8; radius: 4
                            color: root.stateColor(modelData.state)
                        }

                        // Drag repositions the camera on the canvas (P3-01's
                        // drag verb, persisted); a plain click focuses it. The
                        // drag threshold keeps the two distinct, and a completed
                        // drag suppresses the click.
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            drag.target: sTile
                            drag.threshold: 3
                            onReleased: {
                                if (!drag.active) return
                                var wx = (sTile.x + sTile.width / 2
                                          - spatialView.offX) / spatialView.zoom
                                var wy = (sTile.y + sTile.height / 2
                                          - spatialView.offY) / spatialView.zoom
                                // The COMMITTED drag is one audited command
                                // (the per-frame motion is not — see the
                                // coverage check's documented exemptions).
                                root.cmd("workspace.place",
                                         { "tile": modelData.id, "x": wx, "y": wy })
                                spatialView.markUnsettled()
                            }
                            onClicked: root.cmd("workspace.focus",
                                                { "tile": modelData.id })
                        }
                    }
                }

                // Canvas control: fit everything back into view.
                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 14
                    width: fitText.width + 22; height: 26; radius: 6
                    color: "#1a1f28"; border.color: "#333c4c"; border.width: 1
                    Text {
                        id: fitText; anchors.centerIn: parent
                        text: "⤢ Fit"; color: "#cbd3df"; font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: spatialView.fitToView()
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 14
                    width: premisesText.width + 22
                    height: 42
                    radius: 6
                    color: "#1a1f28"
                    border.color: "#333c4c"
                    Text {
                        id: premisesText
                        anchors.centerIn: parent
                        text: ((typeof premises !== "undefined") && premises)
                              ? premises.siteName + " / " + premises.floorName
                                + "\n" + premises.timezone
                              : "Premises unavailable"
                        color: "#cbd3df"
                        font.pixelSize: 11
                    }
                }
            }

            // Layered "selected-camera information" panel, floating over the
            // grid (P3-14: panels layer over the camera-grid background).
            Rectangle {
                id: infoPanel
                property var sel: (governor.focusIndex >= 0
                                   && governor.tiles.length > governor.focusIndex)
                                  ? governor.tiles[governor.focusIndex] : null
                property var diag: sel && governor.mediaDiagnostics.length
                                        > governor.focusIndex
                                   ? governor.mediaDiagnostics[governor.focusIndex]
                                   : null
                property bool showInstant: root.tabIndex === 0
                                           && (typeof instant !== "undefined") && instant
                visible: sel !== null
                width: 348
                height: showInstant ? 350 : 318
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 16
                radius: 10
                color: Qt.rgba(0.055, 0.063, 0.078, 0.94)
                border.width: 2
                border.color: sel ? root.stateColor(sel.state) : "#3a4150"

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 5
                    Text {
                        text: infoPanel.sel ? ("Tile #" + infoPanel.sel.id + "  ·  selected")
                                            : ""
                        color: "#e8ecf3"; font.pixelSize: 15; font.bold: true
                    }
                    Text {
                        text: infoPanel.sel ? (infoPanel.sel.tier + "  ·  "
                                               + infoPanel.sel.stateText) : ""
                        color: infoPanel.sel ? root.stateColor(infoPanel.sel.state)
                                             : "#9aa4b4"
                        font.pixelSize: 13
                    }
                    // Desired media tier (the first control axis): the quality
                    // ceiling the governor will not exceed. Off frees this
                    // camera's budget entirely for the others.
                    Row {
                        spacing: 5
                        Text {
                            text: "quality"
                            color: "#9aa4b4"; font.pixelSize: 12
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Repeater {
                            model: [
                                { label: "Main",  lvl: 3, key: "main" },
                                { label: "Sub",   lvl: 2, key: "sub" },
                                { label: "Thumb", lvl: 1, key: "thumb" },
                                { label: "Off",   lvl: 0, key: "off" }
                            ]
                            delegate: Rectangle {
                                property bool active: infoPanel.sel
                                    && infoPanel.sel.desired === modelData.key
                                width: 44; height: 22; radius: 5
                                color: active ? "#2a4258" : "#1a1f28"
                                border.color: active ? "#3a6ea5" : "#333c4c"
                                border.width: 1
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: active ? "#e8ecf3" : "#9aa4b4"
                                    font.pixelSize: 11
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.cmd(
                                        "workspace.quality",
                                        { "tile": governor.focusIndex,
                                          "tier": modelData.key })
                                }
                            }
                        }
                    }

                    // Device-activity priority (the second control axis): sets how
                    // hard the governor protects this camera's tier under pressure,
                    // independent of which tile is focused. Persists across sweeps.
                    Row {
                        spacing: 6
                        Text {
                            text: "priority"
                            color: "#9aa4b4"; font.pixelSize: 12
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Repeater {
                            model: [
                                { label: "High", lvl: 3, key: "high" },
                                { label: "Med",  lvl: 2, key: "medium" },
                                { label: "Low",  lvl: 1, key: "low" }
                            ]
                            delegate: Rectangle {
                                property bool active: infoPanel.sel
                                    && infoPanel.sel.priority === modelData.key
                                width: 44; height: 22; radius: 5
                                color: active ? "#2f6f4a" : "#1a1f28"
                                border.color: active ? "#37c871" : "#333c4c"
                                border.width: 1
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: active ? "#e8ecf3" : "#9aa4b4"
                                    font.pixelSize: 11
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.cmd(
                                        "workspace.priority",
                                        { "tile": governor.focusIndex,
                                          "level": modelData.key })
                                }
                            }
                        }
                    }
                    Rectangle {
                        width: parent.width
                        height: 1
                        color: "#2c3441"
                    }
                    Text {
                        objectName: "selectedDiagnosticsTitle"
                        text: "Live diagnostics"
                        color: "#cbd3df"
                        font.pixelSize: 12
                        font.bold: true
                    }
                    Text {
                        width: parent.width
                        text: infoPanel.diag
                              ? ("stream  " + infoPanel.diag.streamStateText
                                 + " · " + infoPanel.diag.streamReason)
                              : "stream  Unavailable"
                        color: infoPanel.diag
                               && infoPanel.diag.streamState === "playing"
                               ? "#62d98b" : "#d1a15c"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: {
                            if (!infoPanel.diag) return "media  Unavailable"
                            var parts = []
                            parts.push(infoPanel.diag.codecAvailable
                                       ? infoPanel.diag.codec : "codec unavailable")
                            parts.push(infoPanel.diag.resolutionAvailable
                                       ? (infoPanel.diag.width + "×"
                                          + infoPanel.diag.height)
                                       : "resolution unavailable")
                            parts.push(infoPanel.diag.fpsAvailable
                                       ? (Number(infoPanel.diag.fps).toFixed(1)
                                          + " fps")
                                       : "fps unavailable")
                            return "media  " + parts.join(" · ")
                        }
                        color: "#9fb3c8"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: infoPanel.diag && infoPanel.diag.bitrateAvailable
                              ? ("bitrate  "
                                 + Number(infoPanel.diag.bitrateKbps).toFixed(0)
                                 + " kb/s")
                              : "bitrate  Unavailable · no transport telemetry"
                        color: "#7f8998"; font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: infoPanel.diag && infoPanel.diag.latencyAvailable
                              ? ("latency  "
                                 + Number(infoPanel.diag.latencyMs).toFixed(0)
                                 + " ms")
                              : "latency  Unavailable · no source timestamp"
                        color: "#7f8998"; font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: infoPanel.diag && infoPanel.diag.packetLossAvailable
                              ? ("packet loss  "
                                 + Number(infoPanel.diag.packetLossPct).toFixed(2)
                                 + "%")
                              : "packet loss  Unavailable · no RTP/RTCP stats"
                        color: "#7f8998"; font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    // Instant replay (P3-05 / inc 23): jump back N seconds on the
                    // recording-backed camera and watch, then return to live. Only
                    // on the Live tab and only when a recording index is present.
                    Row {
                        spacing: 6
                        visible: infoPanel.showInstant
                        Text {
                            text: "replay"
                            color: "#9aa4b4"; font.pixelSize: 12
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Repeater {
                            model: [ { label: "10s", s: 10 },
                                     { label: "30s", s: 30 },
                                     { label: "60s", s: 60 } ]
                            delegate: Rectangle {
                                width: 44; height: 22; radius: 5
                                color: "#25324a"
                                border.color: "#3a6ea5"; border.width: 1
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: "#cfe0f2"; font.pixelSize: 11
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.cmd("replay.start",
                                                        { "seconds": modelData.s })
                                }
                            }
                        }
                    }
                    Text {
                        text: "click a tile to focus · set priority to pin importance"
                        color: "#6b7482"; font.pixelSize: 11
                    }
                }
            }

            // P3-18 / inc 35: read-only premises operations aggregate on the
            // front layer. Operating hours are evaluated from the persisted
            // site schedule; other missing sources stay explicitly unavailable.
            Rectangle {
                id: siteOperationsPanel
                objectName: "siteOperationsPanel"
                property var ops: (typeof siteOps !== "undefined") ? siteOps : null
                property var snap: ops ? ops.snapshot : ({})
                property var site: snap.site || ({})
                property var clock: snap.localClock || ({})
                property var hours: snap.operatingHours || ({})
                property var uptime: snap.uptimeLastSeen || ({})
                property var duration: snap.cumulativeDuration || ({})
                property var device: snap.devices || ({})
                property var media: snap.media || ({})
                property var analysis: snap.analysis || ({})
                visible: root.tabIndex === 0 && root.showSiteOps && ops !== null
                width: 390
                height: 420
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: root.showBrowser ? 264 : 16
                anchors.topMargin: root.spatialMode ? 52 : 16
                radius: 10
                color: Qt.rgba(0.055, 0.063, 0.078, 0.96)
                border.width: 1
                border.color: "#354155"

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 5
                    Text {
                        text: "Premises operations"
                        color: "#e8ecf3"; font.pixelSize: 15; font.bold: true
                    }
                    Text {
                        width: parent.width
                        text: siteOperationsPanel.site.available
                              ? (siteOperationsPanel.site.name + " · "
                                 + siteOperationsPanel.site.floor)
                              : "Premises unavailable"
                        color: "#9fb3c8"; font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: siteOperationsPanel.clock.available
                              ? (siteOperationsPanel.clock.date + "  "
                                 + siteOperationsPanel.clock.time + "  "
                                 + siteOperationsPanel.clock.zone + " · "
                                 + siteOperationsPanel.site.timezone)
                              : ("Local time unavailable · "
                                 + (siteOperationsPanel.clock.reason || "no timezone"))
                        color: siteOperationsPanel.clock.available
                               ? "#62d98b" : "#d1a15c"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Rectangle { width: parent.width; height: 1; color: "#2c3441" }
                    Text {
                        text: "Operations time"
                        color: "#cbd3df"; font.pixelSize: 12; font.bold: true
                    }
                    Text {
                        width: parent.width
                        text: siteOperationsPanel.hours.available
                              ? (siteOperationsPanel.hours.stateText + " · "
                                 + siteOperationsPanel.hours.todayHours + " · "
                                 + siteOperationsPanel.hours.source
                                 + (siteOperationsPanel.hours.label
                                    ? (" (" + siteOperationsPanel.hours.label + ")")
                                    : ""))
                              : ("Open/closed  Unavailable · "
                                 + (siteOperationsPanel.hours.reason || ""))
                        color: siteOperationsPanel.hours.available
                               ? (siteOperationsPanel.hours.open
                                  ? "#62d98b" : "#d1a15c")
                               : "#7f8998"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: siteOperationsPanel.uptime.available
                              ? (siteOperationsPanel.uptime.stateText + " · last seen "
                                 + (siteOperationsPanel.uptime.latestLastSeenUtc
                                    || "not yet") + " UTC")
                              : ("Uptime / last seen  Unavailable · "
                                 + (siteOperationsPanel.uptime.reason || ""))
                        color: siteOperationsPanel.uptime.available
                               ? "#9fb3c8" : "#7f8998"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: siteOperationsPanel.duration.available
                              ? (siteOperationsPanel.duration.stateText
                                 + (siteOperationsPanel.duration.recordingAvailable
                                    ? (" · recorded "
                                       + siteOperationsPanel.duration.recordingText
                                       + " / " + siteOperationsPanel.duration.segments
                                       + " segment"
                                       + (siteOperationsPanel.duration.segments === 1
                                          ? "" : "s"))
                                    : " · recording unavailable")
                                 + (siteOperationsPanel.duration.overlapRemovedSeconds > 0
                                    ? (" · "
                                       + siteOperationsPanel.duration.overlapRemovedSeconds
                                       + "s overlap removed") : "")
                                 + (siteOperationsPanel.duration.streamingAvailable
                                    ? (" · streamed "
                                       + siteOperationsPanel.duration.streamingText
                                       + " / "
                                       + siteOperationsPanel.duration.streamingCheckpoints
                                       + " checkpoint"
                                       + (siteOperationsPanel.duration.streamingCheckpoints === 1
                                          ? "" : "s"))
                                    : " · streaming unavailable"))
                              : ("Cumulative duration  Unavailable · "
                                 + (siteOperationsPanel.duration.reason || ""))
                        color: siteOperationsPanel.duration.available
                               ? "#9fb3c8" : "#7f8998"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Rectangle { width: parent.width; height: 1; color: "#2c3441" }
                    Text {
                        text: "Devices"
                        color: "#cbd3df"; font.pixelSize: 12; font.bold: true
                    }
                    Text {
                        width: parent.width
                        text: siteOperationsPanel.device.available
                              ? (siteOperationsPanel.device.total + " total · "
                                 + siteOperationsPanel.device.online + " online · "
                                 + siteOperationsPanel.device.degraded + " degraded · "
                                 + siteOperationsPanel.device.offline + " offline"
                                 + (siteOperationsPanel.device.unassigned > 0
                                    ? (" · " + siteOperationsPanel.device.unassigned
                                       + " unassigned") : ""))
                              : "Device inventory unavailable"
                        color: "#9fb3c8"; font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: siteOperationsPanel.device.available
                              ? (siteOperationsPanel.device.unknown + " unknown · "
                                 + siteOperationsPanel.device.unsupported
                                 + " unsupported · "
                                 + siteOperationsPanel.device.detached + " detached · "
                                 + siteOperationsPanel.device.maintenance
                                 + " maintenance · "
                                 + siteOperationsPanel.device.attention + " attention")
                              : (siteOperationsPanel.device.reason || "Unavailable")
                        color: siteOperationsPanel.device.attention > 0
                               ? "#e6a39b" : "#7f8998"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Rectangle { width: parent.width; height: 1; color: "#2c3441" }
                    Text {
                        text: "Governed media"
                        color: "#cbd3df"; font.pixelSize: 12; font.bold: true
                    }
                    Text {
                        width: parent.width
                        text: siteOperationsPanel.media.available
                              ? (siteOperationsPanel.media.playing + " playing · "
                                 + siteOperationsPanel.media.connecting + " connecting · "
                                 + siteOperationsPanel.media.stalled + " stalled · "
                                 + siteOperationsPanel.media.paused + " paused"
                                 + (siteOperationsPanel.media.displayFpsAvailable
                                    ? (" · " + Number(siteOperationsPanel.media.displayFps)
                                       .toFixed(1) + " display fps") : ""))
                              : ("Unavailable · "
                                 + (siteOperationsPanel.media.reason || "no media source"))
                        color: siteOperationsPanel.media.available
                               ? "#9fb3c8" : "#7f8998"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Rectangle { width: parent.width; height: 1; color: "#2c3441" }
                    Text {
                        text: "Analysis"
                        color: "#cbd3df"; font.pixelSize: 12; font.bold: true
                    }
                    Text {
                        width: parent.width
                        text: siteOperationsPanel.analysis.available
                              ? ((siteOperationsPanel.analysis.stateText || "No active alarms")
                                 + " · H " + siteOperationsPanel.analysis.high
                                 + " / M " + siteOperationsPanel.analysis.medium
                                 + " / L " + siteOperationsPanel.analysis.low)
                              : ("Unavailable · "
                                 + (siteOperationsPanel.analysis.reason || "no source"))
                        color: siteOperationsPanel.analysis.needsAttention > 0
                               ? "#e6a39b" : "#9fb3c8"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: siteOperationsPanel.analysis.available
                              ? (siteOperationsPanel.analysis.affectedDevices
                                 + " affected devices · "
                                 + siteOperationsPanel.analysis.occurrences
                                 + " occurrences · session only")
                              : ""
                        color: "#7f8998"; font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }
            }

            // Layered instant-replay overlay (P3-05 / inc 23): floats over the
            // live wall — which keeps running behind it (P3-14 layering) — showing
            // the look-back footage with a compact transport and a return-to-live
            // control. Honest when the camera has no local recording.
            Rectangle {
                id: replayOverlay
                property var ir: (typeof instant !== "undefined") ? instant : null
                property var ipb: ir ? ir.pb : null
                visible: ir && ir.active && root.tabIndex === 0
                z: 60
                anchors.centerIn: parent
                width: Math.min(parent.width - 80, 900)
                height: Math.min(parent.height - 80, 560)
                radius: 12
                color: Qt.rgba(0.043, 0.051, 0.063, 0.97)
                border.width: 2
                border.color: (ir && ir.available) ? "#3a6ea5" : "#e0785a"

                function spanColor(s) {
                    switch (s) {
                    case "available":   return "#37c871";
                    case "overlapping": return "#f2a33c";
                    }
                    return "#2a303c";   // missing gap
                }

                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    // Header: title + honest status + return-to-live.
                    Item {
                        width: parent.width
                        height: 26
                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Instant replay"
                            color: "#e8ecf3"; font.pixelSize: 16; font.bold: true
                        }
                        Text {
                            anchors.centerIn: parent
                            text: replayOverlay.ir ? replayOverlay.ir.status : ""
                            color: (replayOverlay.ir && replayOverlay.ir.available)
                                   ? "#9aa4b4" : "#e0785a"
                            font.pixelSize: 13
                        }
                        Rectangle {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: liveBackText.width + 24; height: 26; radius: 6
                            color: "#25324a"; border.color: "#3a6ea5"; border.width: 1
                            Text {
                                id: liveBackText; anchors.centerIn: parent
                                text: "⟵ Live"; color: "#cfe0f2"; font.pixelSize: 13
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.cmd("replay.live", {})
                            }
                        }
                    }

                    // Video slot: the decoded replay, or an honest no-footage note.
                    Rectangle {
                        id: instantVideoSlot
                        width: parent.width
                        height: parent.height - 120
                        color: "#0b0d11"
                        radius: 8
                        clip: true

                        VideoItem {
                            objectName: "instantVideoOut"
                            anchors.fill: parent
                            visible: replayOverlay.ir && replayOverlay.ir.available
                        }
                        Text {
                            anchors.centerIn: parent
                            visible: !(replayOverlay.ir && replayOverlay.ir.available)
                            text: "No local recording for this camera"
                            color: "#e0785a"; font.pixelSize: 15
                        }
                    }

                    // Availability bar + draggable playhead (bound to instant.pb),
                    // the same honest timeline the Playback tab uses.
                    Rectangle {
                        id: replayTrack
                        width: parent.width
                        height: 12
                        radius: 6
                        color: "#171a21"
                        visible: replayOverlay.ir && replayOverlay.ir.available

                        Repeater {
                            model: replayOverlay.ipb ? replayOverlay.ipb.spans : []
                            delegate: Rectangle {
                                height: parent.height
                                y: 0
                                x: modelData.startFrac * replayTrack.width
                                width: Math.max(1, (modelData.endFrac - modelData.startFrac)
                                                    * replayTrack.width)
                                color: replayOverlay.spanColor(modelData.state)
                            }
                        }
                        Rectangle {   // playhead
                            width: 3; height: parent.height + 6; y: -3
                            x: (replayOverlay.ipb ? replayOverlay.ipb.playheadFrac : 0)
                               * replayTrack.width - 1.5
                            color: replayOverlay.ipb && replayOverlay.ipb.onFootage
                                   ? "#e8ecf3" : "#e05a4e"
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onPressed: (mouse) => { if (replayOverlay.ipb)
                                replayOverlay.ipb.seekFrac(mouse.x / replayTrack.width) }
                            onPositionChanged: (mouse) => { if (replayOverlay.ipb)
                                replayOverlay.ipb.seekFrac(Math.max(0, Math.min(1,
                                    mouse.x / replayTrack.width))) }
                            onReleased: root.cmd("replay.seek",
                                { "position": Math.max(0, Math.min(1,
                                    mouse.x / replayTrack.width)) })
                        }
                    }

                    // Transport: restart · play/pause · speed. Bound to instant.pb.
                    Row {
                        spacing: 8
                        visible: replayOverlay.ir && replayOverlay.ir.available
                        Repeater {
                            model: [ { label: "⏮", act: 0 },
                                     { label: "play", act: 1 },
                                     { label: "1×", act: 2, v: 1 },
                                     { label: "2×", act: 2, v: 2 },
                                     { label: "4×", act: 2, v: 4 } ]
                            delegate: Rectangle {
                                property bool active: modelData.act === 1
                                    ? (replayOverlay.ipb && replayOverlay.ipb.playing)
                                    : (modelData.act === 2 && replayOverlay.ipb
                                       && replayOverlay.ipb.speed === modelData.v)
                                width: 46; height: 26; radius: 6
                                color: active ? "#2a4258" : "#1a1f28"
                                border.color: active ? "#3a6ea5" : "#333c4c"
                                border.width: 1
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.act === 1
                                          ? (replayOverlay.ipb && replayOverlay.ipb.playing
                                             ? "⏸" : "▶")
                                          : modelData.label
                                    color: active ? "#e8ecf3" : "#9aa4b4"
                                    font.pixelSize: 12
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (!replayOverlay.ipb) return
                                        if (modelData.act === 0)
                                            root.cmd("replay.seek", { "position": 0 })
                                        else if (modelData.act === 1)
                                            replayOverlay.ipb.playing
                                                ? root.cmd("replay.pause", {})
                                                : root.cmd("replay.play", {})
                                        else if (modelData.act === 2)
                                            root.cmd("replay.speed", { "rate": modelData.v })
                                    }
                                }
                            }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: replayOverlay.ipb
                                  ? (replayOverlay.ipb.playheadUtc + " UTC") : ""
                            color: "#9aa4b4"; font.pixelSize: 12
                        }
                    }
                }
            }

            // Command palette (inc 25, P1-12/A0), layered over the grid: type a
            // command ("focus 5", "quality 3 thumb", "device.remove cam-1
            // confirm"), it flows through the ONE validated gate, and the
            // deterministic result + the session audit trail (refusals
            // included) show right here.
            Rectangle {
                id: palette
                property var cc: (typeof commander !== "undefined") ? commander : null
                visible: root.showPalette && cc !== null
                z: 70
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 18
                width: Math.min(parent.width - 60, 660)
                height: 320
                radius: 10
                color: Qt.rgba(0.043, 0.051, 0.063, 0.97)
                border.color: "#3a6ea5"; border.width: 1.5

                property string result: ""

                onVisibleChanged: if (visible) cmdInput.forceActiveFocus()

                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 8

                    Item {
                        width: parent.width; height: 22
                        Text {
                            anchors.left: parent.left
                            text: "Command"
                            color: "#e8ecf3"; font.pixelSize: 14; font.bold: true
                        }
                        Text {
                            anchors.right: parent.right
                            text: "every action · one validated, audited gate (A0)"
                            color: "#6b7482"; font.pixelSize: 11
                        }
                    }

                    Rectangle {
                        width: parent.width; height: 30; radius: 6
                        color: "#10141b"; border.color: "#333c4c"; border.width: 1
                        TextInput {
                            id: cmdInput
                            anchors.fill: parent
                            anchors.margins: 7
                            color: "#e8ecf3"; font.pixelSize: 13
                            font.family: "Consolas"
                            clip: true
                            onAccepted: {
                                if (!palette.cc || text.trim().length === 0) return
                                palette.result = palette.cc.run(text)
                                text = ""
                            }
                            Text {
                                anchors.fill: parent
                                visible: cmdInput.text.length === 0
                                text: "focus 5 · quality 3 thumb · layout 16 · replay.start 30 …"
                                color: "#4a5261"; font.pixelSize: 12
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        visible: palette.result.length > 0
                        text: palette.result
                        color: palette.result.startsWith("ok") ? "#37c871" : "#e0785a"
                        font.pixelSize: 12
                        font.family: "Consolas"
                        elide: Text.ElideRight
                    }

                    // The catalog (machine-discoverable; human-skimmable here).
                    Text {
                        text: "commands"
                        color: "#6b7482"; font.pixelSize: 11
                    }
                    Flickable {
                        width: parent.width
                        height: 74
                        contentHeight: hintCol.height
                        clip: true
                        Column {
                            id: hintCol
                            Repeater {
                                model: palette.cc ? palette.cc.commandHints : []
                                delegate: Text {
                                    text: modelData
                                    color: "#8a93a3"; font.pixelSize: 11
                                    font.family: "Consolas"
                                }
                            }
                        }
                    }

                    // The session audit trail — refused attempts included.
                    Text {
                        text: "audit (this session)"
                        color: "#6b7482"; font.pixelSize: 11
                    }
                    Column {
                        width: parent.width
                        Repeater {
                            model: palette.cc
                                   ? palette.cc.auditLog.slice(0, 4) : []
                            delegate: Text {
                                width: parent.width
                                text: modelData.time + "  " + modelData.command
                                      + (modelData.args.length ? " " + modelData.args : "")
                                      + " → " + modelData.outcome
                                color: modelData.ok ? "#9aa4b4" : "#e0785a"
                                font.pixelSize: 11
                                font.family: "Consolas"
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            // Alarms panel (inc 27, P6-07), layered over the grid: the
            // deduplicated, accountable alarm list. Every lifecycle button
            // routes through the command envelope (alarm.ack / escalate /
            // clear) so the panel, the palette, and the API are one audited
            // verb. Maintenance-suppressed alarms stay visible (state never
            // hidden), only their notification is silenced.
            Rectangle {
                id: alarmsPanel
                property var ac: (typeof alarmsCtrl !== "undefined") ? alarmsCtrl : null
                visible: root.showAlarms && ac !== null
                z: 65
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                width: Math.min(parent.width - 60, 460)
                height: Math.min(parent.height - 60, 380)
                radius: 10
                color: Qt.rgba(0.043, 0.051, 0.063, 0.97)
                border.color: (ac && ac.needsAttention > 0) ? "#e05a4e" : "#333c4c"
                border.width: 1.5

                function prioColor(p) {
                    switch (p) {
                    case "high":   return "#e05a4e";
                    case "medium": return "#f2a33c";
                    }
                    return "#5b6472";
                }

                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 8

                    Item {
                        width: parent.width; height: 22
                        Text {
                            anchors.left: parent.left
                            text: "Alarms"
                            color: "#e8ecf3"; font.pixelSize: 14; font.bold: true
                        }
                        Text {
                            anchors.right: parent.right
                            text: alarmsPanel.ac
                                  ? (alarmsPanel.ac.needsAttention > 0
                                     ? alarmsPanel.ac.needsAttention + " need attention"
                                     : "all quiet")
                                  : ""
                            color: alarmsPanel.ac && alarmsPanel.ac.needsAttention > 0
                                   ? "#e0785a" : "#6b7482"
                            font.pixelSize: 11
                        }
                    }

                    Text {
                        visible: alarmsPanel.ac && alarmsPanel.ac.alarms.length === 0
                        text: "No active alarms."
                        color: "#6b7482"; font.pixelSize: 12
                    }

                    Flickable {
                        width: parent.width
                        height: parent.height - 40
                        contentHeight: alarmCol.height
                        clip: true
                        Column {
                            id: alarmCol
                            width: parent.width
                            spacing: 6
                            Repeater {
                                model: alarmsPanel.ac ? alarmsPanel.ac.alarms : []
                                delegate: Rectangle {
                                    id: alarmRow
                                    // The inner buttons' Repeater shadows
                                    // modelData; capture the alarm's id here.
                                    property double alarmId: modelData.id
                                    width: alarmCol.width
                                    height: 62
                                    radius: 8
                                    color: "#10141b"
                                    border.color: alarmsPanel.prioColor(modelData.priority)
                                    border.width: 1
                                    opacity: modelData.suppressed ? 0.55 : 1.0

                                    Column {
                                        anchors.left: parent.left
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.leftMargin: 10
                                        spacing: 3
                                        Text {
                                            text: "#" + modelData.id + "  " + modelData.device
                                                  + "  ·  " + modelData.priority.toUpperCase()
                                                  + "  ·  " + modelData.state
                                                  + (modelData.count > 1
                                                     ? "  ·  ×" + modelData.count : "")
                                                  + (modelData.suppressed ? "  ·  maintenance" : "")
                                            color: "#e8ecf3"; font.pixelSize: 12; font.bold: true
                                        }
                                        Text {
                                            text: modelData.time + " UTC  ·  " + modelData.message
                                            color: "#8a93a3"; font.pixelSize: 11
                                        }
                                    }
                                    Row {
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.rightMargin: 8
                                        spacing: 5
                                        Repeater {
                                            model: [ { label: "Ack", verb: "ack" },
                                                     { label: "Esc", verb: "escalate" },
                                                     { label: "Clear", verb: "clear" } ]
                                            delegate: Rectangle {
                                                width: 40; height: 22; radius: 5
                                                color: "#1a1f28"
                                                border.color: "#333c4c"; border.width: 1
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: modelData.label
                                                    color: "#9aa4b4"; font.pixelSize: 10
                                                }
                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor
                                                    // Route through the envelope (A0).
                                                    onClicked: root.alarmAction(
                                                        modelData.verb,
                                                        alarmRow.alarmId)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Layered "resource browser": the camera list + search, over the grid
            // (P3-14). Clicking a row focuses that camera, exactly like clicking
            // its tile; the focused camera is highlighted. Toggled from the
            // toolbar's ☰ Cameras button.
            Rectangle {
                id: browser
                visible: root.showBrowser
                width: 248
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                color: Qt.rgba(0.043, 0.051, 0.063, 0.95)
                border.color: "#232a36"
                border.width: 1

                Text {
                    id: browserTitle
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 14
                    text: "Cameras (" + governor.tiles.length + ")"
                    color: "#e8ecf3"; font.pixelSize: 15; font.bold: true
                }

                Rectangle {
                    id: searchBox
                    anchors.top: browserTitle.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.topMargin: 10
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    height: 32
                    radius: 6
                    color: "#11151c"
                    border.color: searchInput.activeFocus ? "#3a6ea5" : "#2a3240"
                    border.width: 1

                    TextInput {
                        id: searchInput
                        anchors.fill: parent
                        anchors.leftMargin: 9
                        anchors.rightMargin: 9
                        verticalAlignment: TextInput.AlignVCenter
                        color: "#e8ecf3"; font.pixelSize: 13
                        clip: true
                        selectByMouse: true
                        onTextChanged: root.cameraSearch = text
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: searchInput.text.length === 0
                            text: "search cameras…"
                            color: "#5a6270"; font.pixelSize: 13
                        }
                    }
                }

                ListView {
                    id: camList
                    anchors.top: searchBox.bottom
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.topMargin: 10
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    anchors.bottomMargin: 8
                    clip: true
                    spacing: 4
                    model: root.filterTiles(governor.tiles, root.cameraSearch)

                    delegate: Rectangle {
                        width: camList.width
                        height: 46
                        radius: 6
                        color: modelData.focused ? Qt.rgba(0.16, 0.22, 0.33, 0.6)
                                                 : Qt.rgba(1, 1, 1, 0.03)
                        border.color: modelData.focused
                                      ? root.stateColor(modelData.state) : "transparent"
                        border.width: modelData.focused ? 2 : 0

                        Rectangle {
                            id: dot
                            width: 10; height: 10; radius: 5
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            color: root.stateColor(modelData.state)
                        }
                        Column {
                            anchors.left: dot.right
                            anchors.leftMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 1
                            Text {
                                text: "Cam " + modelData.id
                                      + (modelData.priority === "high" ? "  ★" : "")
                                color: modelData.priority === "high" ? "#f2c94c" : "#e8ecf3"
                                font.pixelSize: 13
                                font.bold: modelData.focused
                            }
                            Text {
                                text: modelData.tier + " · " + modelData.stateText
                                color: "#9aa4b4"; font.pixelSize: 11
                            }
                        }
                        Text {
                            visible: modelData.focused
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: "◉"
                            color: "#e8ecf3"; font.pixelSize: 12
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.cmd("workspace.focus",
                                                { "tile": modelData.id })
                        }
                    }
                }
            }

            // Playback transport, layered at the bottom on the Playback tab
            // (P3-14 · P5-02). The timeline is bound to the real recording index
            // via `playback`: green = recorded footage, amber = overlapping
            // (duplicate) footage, and the dark track shows honestly through gaps
            // where nothing was recorded — requested time is NEVER painted as
            // recorded. The playhead is draggable; it reports the exact recorded
            // time and whether it sits on footage or in a gap, and which file
            // backs it. (Decoding those pixels into the pane is the next slice.)
            Rectangle {
                id: transport
                visible: root.tabIndex === 1
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 118
                color: Qt.rgba(0.03, 0.035, 0.045, 0.97)
                border.color: "#232a36"; border.width: 1
                property var pb: (typeof playback !== "undefined") ? playback : null

                function spanColor(s) {
                    switch (s) {
                    case "available":   return "#37c871";
                    case "overlapping": return "#f2a33c";
                    }
                    return "transparent";   // missing gap: the dark track shows through
                }

                Rectangle {
                    id: track
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.topMargin: 18
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    height: 16; radius: 4; color: "#11151c"
                    border.color: "#242c39"; border.width: 1
                    clip: true

                    // Availability bands from the recording index.
                    Repeater {
                        model: transport.pb ? transport.pb.spans : []
                        delegate: Rectangle {
                            x: modelData.startFrac * track.width
                            width: Math.max(1, (modelData.endFrac - modelData.startFrac) * track.width)
                            height: track.height
                            radius: 2
                            color: transport.spanColor(modelData.state)
                        }
                    }

                    // Draggable playhead.
                    Rectangle {
                        visible: transport.pb !== null
                        x: (transport.pb ? transport.pb.playheadFrac : 0) * track.width - 1.5
                        y: -5; width: 3; height: track.height + 10; radius: 1.5
                        color: transport.pb && transport.pb.onFootage ? "#e8ecf3" : "#e05a4e"
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        enabled: transport.pb !== null
                        onPressed: (mouse) => { if (transport.pb)
                            transport.pb.seekFrac(mouse.x / track.width) }
                        onPositionChanged: (mouse) => { if (transport.pb)
                            transport.pb.seekFrac(Math.max(0, Math.min(1, mouse.x / track.width))) }
                        onReleased: root.cmd("playback.seek",
                            { "position": Math.max(0, Math.min(1,
                                mouse.x / track.width)) })
                    }
                }

                // Readout: the recorded time under the playhead + footage/gap state.
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 20
                    anchors.top: track.bottom
                    anchors.topMargin: 8
                    text: transport.pb
                          ? (transport.pb.playheadUtc + " UTC   ·   "
                             + (transport.pb.onFootage ? "on recorded footage"
                                                       : "no footage at this time"))
                          : "no recording index (run vms_record, or pass --rec-db)"
                    color: transport.pb && transport.pb.onFootage ? "#9aa4b4" : "#e0785a"
                    font.pixelSize: 12
                }

                // Transport controls + speed.
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 12
                    spacing: 10
                    Repeater {
                        model: 5
                        delegate: Rectangle {
                            width: 42; height: 30; radius: 6
                            color: "#161b24"; border.color: "#2a3240"; border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: index === 0 ? "⏮"
                                    : index === 1 ? "◀ᖴ"
                                    : index === 2 ? (transport.pb && transport.pb.playing ? "⏸" : "▶")
                                    : index === 3 ? "ᖴ▶" : "⏭"
                                color: "#cbd3df"; font.pixelSize: 14
                            }
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                enabled: transport.pb !== null
                                onClicked: {
                                    if (!transport.pb) return
                                    if (index === 0)
                                        root.cmd("playback.seek", { "position": 0 })
                                    else if (index === 1)
                                        root.cmd("playback.step", { "frames": -25 })
                                    else if (index === 2) transport.pb.playing
                                                          ? root.cmd("playback.pause", {})
                                                          : root.cmd("playback.play", {})
                                    else if (index === 3)
                                        root.cmd("playback.step", { "frames": 25 })
                                    else root.cmd("playback.seek", { "position": 1 })
                                }
                            }
                        }
                    }

                    Rectangle { width: 1; height: 24; color: "#2a3240"
                        anchors.verticalCenter: parent.verticalCenter }

                    Repeater {
                        model: [ { label: "1×", v: 1 }, { label: "2×", v: 2 }, { label: "4×", v: 4 } ]
                        delegate: Rectangle {
                            property bool active: transport.pb && transport.pb.speed === modelData.v
                            width: 34; height: 30; radius: 6
                            color: active ? "#2a4258" : "#161b24"
                            border.color: active ? "#3a6ea5" : "#2a3240"; border.width: 1
                            Text {
                                anchors.centerIn: parent; text: modelData.label
                                color: active ? "#e8ecf3" : "#9aa4b4"; font.pixelSize: 12
                            }
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                enabled: transport.pb !== null
                                onClicked: if (transport.pb)
                                    root.cmd("playback.speed", { "rate": modelData.v })
                            }
                        }
                    }
                }
            }
        }

        // --- Devices tab (onboarding UI, increment 14) ---
        // A non-governed management surface, shown only on tabIndex 2. Loaded
        // (not just hidden) so its bindings don't evaluate against a null
        // devicesCtrl on a build without persistence; when present it is
        // instantiated up front so the offscreen --smoke-ms verifies its
        // bindings even while another tab is active.
        Loader {
            id: devicesLoader
            visible: root.tabIndex === 2
            width: parent.width
            height: root.height - tabBar.height
            active: (typeof devicesCtrl !== "undefined") && devicesCtrl !== null
            source: active ? "DevicesView.qml" : ""
        }

        // Fallback when persistence (and thus the device inventory) is absent.
        Rectangle {
            visible: root.tabIndex === 2 && !devicesLoader.active
            width: parent.width
            height: root.height - tabBar.height
            color: "#0e1014"
            Text {
                anchors.centerIn: parent
                text: "Device management needs the persistence build."
                color: "#5a6270"; font.pixelSize: 14
            }
        }
    }
}
