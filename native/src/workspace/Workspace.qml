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

    function stateColor(s) {
        switch (s) {
        case "live":             return "#37c871";
        case "degraded":         return "#f2a33c";
        case "paused-capacity":  return "#e05a4e";
        case "paused-offscreen": return "#5b6472";
        }
        return "#8a93a3";
    }

    Column {
        anchors.fill: parent
        spacing: 0

        // --- header / capacity meter ---
        Rectangle {
            id: header
            width: parent.width
            height: 84
            color: "#141821"

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

            // Legend
            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 20
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
            height: parent.height - header.height

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
                visible: videoActive
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
                        // through the chrome; opaque in the state-only mode.
                        color: videoActive ? Qt.rgba(0.055, 0.063, 0.078, 0.32)
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
                    }
                }
            }
        }
    }
}
