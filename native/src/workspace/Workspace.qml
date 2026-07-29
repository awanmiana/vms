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
    property string cameraSearch: ""

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
                             { label: "Playback", idx: 1 } ]
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
                      : ((typeof playback !== "undefined" && playback)
                         ? ("playback — " + playback.spans.length + " footage span(s)")
                         : "playback — no recording index")
                color: "#5a6270"; font.pixelSize: 11
            }
        }

        // --- header / capacity meter + toolbar ---
        Rectangle {
            id: header
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
                            onClicked: governor.setTileCount(modelData)
                        }
                    }
                }

                Rectangle { width: 1; height: 24; color: "#2a3240"
                    anchors.verticalCenter: parent.verticalCenter }

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
                        onClicked: governor.setAutoSweep(!governor.autoSweeping)
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
                          : "focus on tile " + governor.focusIndex
                            + "  ·  " + governor.columns + "×" + governor.rows + " grid"
                    color: governor.overflow ? "#e05a4e" : "#6f7a86"
                    font.pixelSize: 13
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
                visible: videoActive && root.tabIndex === 0
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
                            onClicked: governor.focusTile(modelData.id)
                        }
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
                visible: sel !== null
                width: 288
                height: 182
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
                                    onClicked: governor.setDesiredTier(governor.focusIndex,
                                                                       modelData.lvl)
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
                                    onClicked: governor.setPriority(governor.focusIndex,
                                                                    modelData.lvl)
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
                            onClicked: governor.focusTile(modelData.id)
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
                                    if (index === 0) transport.pb.seekFrac(0)
                                    else if (index === 1) transport.pb.stepFrames(-25)
                                    else if (index === 2) transport.pb.playing
                                                          ? transport.pb.pause() : transport.pb.play()
                                    else if (index === 3) transport.pb.stepFrames(25)
                                    else transport.pb.seekFrac(1)
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
                                onClicked: if (transport.pb) transport.pb.setSpeed(modelData.v)
                            }
                        }
                    }
                }
            }
        }
    }
}
