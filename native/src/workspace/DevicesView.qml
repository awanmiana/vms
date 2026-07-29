// Onboarding UI — native increment 14 (owner item #3). The Devices tab surface.
//
// Layers a device-management view over the same dark workspace canvas: an
// onboard form on the left, and the onboarded inventory on the right with each
// device's HONEST health (increment 13) — green Online, amber Degraded, red
// Offline, slate Unsupported, grey Unknown-until-observed — plus its exception
// lifecycle (acknowledge silences the alert, not the state), a maintenance-
// window toggle, and remove. Everything binds to the C++ `devicesCtrl`
// (DeviceController) which wraps the DeviceRepo + HealthMonitor cores.
//
// Pure QtQuick primitives (no QtQuick.Controls), matching the rest of the
// workspace, so it verifies headlessly offscreen. The device list uses a
// Repeater (not a lazy ListView) so every delegate — and its bindings —
// instantiates even when the tab is not the active one, which is what makes the
// offscreen --smoke-ms binding check meaningful.

import QtQuick

Item {
    id: root

    // Selected onboarding workflow in the form: "camera" (single-channel direct
    // IP camera) or a recorder ("nvr" / "dvr" / "hybrid", multi-channel).
    property string selectedKind: "camera"
    readonly property bool isRecorder: selectedKind !== "camera"

    // Short human label for a device kind badge.
    function kindLabel(k) {
        switch (k) {
        case "nvr":    return "NVR";
        case "dvr":    return "DVR";
        case "hybrid": return "Hybrid DVR";
        }
        return "IP Camera";
    }

    // Honest health colour, matching the per-tile state vocabulary elsewhere.
    function healthColor(s) {
        switch (s) {
        case "online":      return "#37c871";
        case "degraded":    return "#f2a33c";
        case "offline":     return "#e05a4e";
        case "unsupported": return "#5b6472";
        }
        return "#8a93a3";   // unknown
    }

    // A labelled single-line text field (reused for every onboard input).
    component LabeledField: Column {
        property alias text: input.text
        property string label: ""
        property string placeholder: ""
        property bool secret: false
        spacing: 4
        width: parent ? parent.width : 240

        Text { text: parent.label; color: "#9aa4b4"; font.pixelSize: 11 }
        Rectangle {
            width: parent.width; height: 30; radius: 6
            color: "#11151c"
            border.color: input.activeFocus ? "#3a6ea5" : "#2a3240"
            border.width: 1
            TextInput {
                id: input
                anchors.fill: parent
                anchors.leftMargin: 9
                anchors.rightMargin: 9
                verticalAlignment: TextInput.AlignVCenter
                color: "#e8ecf3"; font.pixelSize: 13
                clip: true
                selectByMouse: true
                echoMode: parent.parent.secret ? TextInput.Password
                                                : TextInput.Normal
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: input.text.length === 0
                    text: input.parent.parent.placeholder
                    color: "#5a6270"; font.pixelSize: 13
                }
            }
        }
    }

    // A small pill button (label + click). `tone` colours the active/emphasis.
    component PillButton: Rectangle {
        property string label: ""
        property color tone: "#3a6ea5"
        signal clicked()
        width: btnText.width + 22; height: 28; radius: 6
        color: "#1a1f28"
        border.color: tone; border.width: 1
        Text {
            id: btnText; anchors.centerIn: parent
            text: parent.label; color: "#cbd3df"; font.pixelSize: 12
        }
        MouseArea {
            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }

    Row {
        anchors.fill: parent
        spacing: 0

        // --- onboard form (left) ---
        Rectangle {
            id: formPanel
            width: 320
            height: parent.height
            color: Qt.rgba(0.043, 0.051, 0.063, 0.97)
            border.color: "#232a36"; border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                Text {
                    text: "Onboard a device"
                    color: "#e8ecf3"; font.pixelSize: 16; font.bold: true
                }
                Text {
                    text: root.isRecorder
                          ? "Recorder (NVR/DVR): channels expand from the URL "
                            + "templates below ({ch} = channel number)."
                          : "Direct IP camera (single channel)."
                    color: "#6f7a86"; font.pixelSize: 11
                    width: parent.width; wrapMode: Text.WordWrap
                }

                // Device-kind selector (P2-01 workflows).
                Row {
                    spacing: 6
                    Repeater {
                        model: [
                            { label: "Camera", key: "camera" },
                            { label: "NVR",    key: "nvr" },
                            { label: "DVR",    key: "dvr" },
                            { label: "Hybrid", key: "hybrid" }
                        ]
                        delegate: Rectangle {
                            property bool active: root.selectedKind === modelData.key
                            width: kindText.width + 18; height: 26; radius: 6
                            color: active ? "#2a4258" : "#1a1f28"
                            border.color: active ? "#3a6ea5" : "#333c4c"; border.width: 1
                            Text {
                                id: kindText; anchors.centerIn: parent
                                text: modelData.label
                                color: active ? "#e8ecf3" : "#9aa4b4"; font.pixelSize: 12
                            }
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: root.selectedKind = modelData.key
                            }
                        }
                    }
                }

                LabeledField { id: fId;      width: parent.width; label: "Device ID *";  placeholder: root.isRecorder ? "e.g. nvr-lobby" : "e.g. cam-front-door" }
                LabeledField { id: fName;    width: parent.width; label: "Name *";       placeholder: root.isRecorder ? "e.g. Lobby NVR" : "e.g. Front Door" }
                LabeledField { id: fAddress; width: parent.width; label: "Address";      placeholder: "host / IP" }
                LabeledField { id: fVendor;  width: parent.width; label: "Vendor";       placeholder: "e.g. Hikvision" }

                Row {
                    width: parent.width; spacing: 10
                    LabeledField { id: fUser; width: (parent.width - 10) / 2; label: "Username";  placeholder: "user" }
                    LabeledField { id: fPass; width: (parent.width - 10) / 2; label: "Password";  placeholder: "••••"; secret: true }
                }

                // Direct-camera stream URLs (single channel).
                Column {
                    width: parent.width; spacing: 12
                    visible: !root.isRecorder
                    LabeledField { id: fMain; width: parent.width; label: "Main stream URL"; placeholder: "rtsp://…/Channels/101" }
                    LabeledField { id: fSub;  width: parent.width; label: "Sub stream URL";  placeholder: "rtsp://…/Channels/102" }
                }

                // Recorder channel expansion (multi-channel).
                Column {
                    width: parent.width; spacing: 12
                    visible: root.isRecorder
                    LabeledField { id: fCount;   width: parent.width; label: "Channel count"; placeholder: "e.g. 8" }
                    LabeledField { id: fMainTpl; width: parent.width; label: "Main URL template"; placeholder: "rtsp://…/Channels/{ch}01" }
                    LabeledField { id: fSubTpl;  width: parent.width; label: "Sub URL template";  placeholder: "rtsp://…/Channels/{ch}02" }
                }

                Row {
                    spacing: 10
                    Rectangle {
                        width: onboardText.width + 30; height: 34; radius: 6
                        color: "#22543a"; border.color: "#37c871"; border.width: 1
                        Text {
                            id: onboardText; anchors.centerIn: parent
                            text: root.isRecorder ? "＋ Onboard recorder" : "＋ Onboard"
                            color: "#e8ecf3"
                            font.pixelSize: 13; font.bold: true
                        }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                var err
                                if (root.isRecorder) {
                                    err = devicesCtrl.onboardRecorder(
                                        fId.text, fName.text, fAddress.text, fVendor.text,
                                        root.selectedKind, fUser.text, fPass.text,
                                        parseInt(fCount.text) || 0, fMainTpl.text, fSubTpl.text)
                                } else {
                                    err = devicesCtrl.onboard(
                                        fId.text, fName.text, fAddress.text, fVendor.text,
                                        fUser.text, fPass.text, fMain.text, fSub.text)
                                }
                                if (err === "") {
                                    fId.text = ""; fName.text = ""; fAddress.text = ""
                                    fVendor.text = ""; fUser.text = ""; fPass.text = ""
                                    fMain.text = ""; fSub.text = ""
                                    fCount.text = ""; fMainTpl.text = ""; fSubTpl.text = ""
                                }
                            }
                        }
                    }
                }

                Text {
                    visible: devicesCtrl.lastError.length > 0
                    text: "⚠ " + devicesCtrl.lastError
                    color: "#e05a4e"; font.pixelSize: 12
                    width: parent.width; wrapMode: Text.WordWrap
                }
            }
        }

        // --- inventory list (right) ---
        Item {
            width: parent.width - formPanel.width
            height: parent.height

            // Header: device count + how many need attention.
            Rectangle {
                id: listHeader
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 52
                color: "transparent"
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Devices (" + devicesCtrl.deviceCount + ")"
                    color: "#e8ecf3"; font.pixelSize: 16; font.bold: true
                }
                Rectangle {
                    visible: devicesCtrl.attentionCount > 0
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    width: attnText.width + 20; height: 24; radius: 12
                    color: Qt.rgba(0.88, 0.35, 0.31, 0.18)
                    border.color: "#e05a4e"; border.width: 1
                    Text {
                        id: attnText; anchors.centerIn: parent
                        text: "⚠ " + devicesCtrl.attentionCount + " need attention"
                        color: "#e05a4e"; font.pixelSize: 12
                    }
                }
            }

            // Honest empty state.
            Text {
                anchors.centerIn: parent
                visible: devicesCtrl.deviceCount === 0
                text: "No devices onboarded yet.\nUse the form to add one."
                horizontalAlignment: Text.AlignHCenter
                color: "#5a6270"; font.pixelSize: 14
            }

            Flickable {
                anchors.top: listHeader.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 12
                clip: true
                contentHeight: cards.height
                contentWidth: width

                Column {
                    id: cards
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: devicesCtrl.devices
                        delegate: Rectangle {
                            width: cards.width
                            height: 92
                            radius: 8
                            color: Qt.rgba(1, 1, 1, 0.03)
                            border.width: 1
                            border.color: modelData.health.needsAttention
                                          ? "#e05a4e" : "#232a36"

                            // health dot
                            Rectangle {
                                id: hdot
                                width: 14; height: 14; radius: 7
                                anchors.left: parent.left
                                anchors.leftMargin: 14
                                anchors.top: parent.top
                                anchors.topMargin: 16
                                color: root.healthColor(modelData.health.state)
                            }

                            // identity + metadata
                            Column {
                                anchors.left: hdot.right
                                anchors.leftMargin: 12
                                anchors.top: parent.top
                                anchors.topMargin: 12
                                anchors.right: actions.left
                                anchors.rightMargin: 12
                                spacing: 2
                                Text {
                                    text: modelData.name + "   ·   " + modelData.health.stateText
                                    color: "#e8ecf3"; font.pixelSize: 14; font.bold: true
                                    elide: Text.ElideRight; width: parent.width
                                }
                                Text {
                                    text: root.kindLabel(modelData.kind)
                                          + "   ·   id " + modelData.id
                                          + (modelData.address ? ("   ·   " + modelData.address) : "")
                                          + (modelData.vendor ? ("   ·   " + modelData.vendor) : "")
                                          + "   ·   " + modelData.cameraCount + " channel(s)"
                                    color: "#8a93a3"; font.pixelSize: 11
                                    elide: Text.ElideRight; width: parent.width
                                }
                                Text {
                                    text: "reach " + modelData.health.reach
                                          + "  ·  stream " + modelData.health.stream
                                          + "  ·  storage " + modelData.health.storage
                                          + (modelData.health.inMaintenance ? "  ·  ⚙ maintenance" : "")
                                    color: "#6f7a86"; font.pixelSize: 11
                                }
                                Text {
                                    visible: modelData.health.exceptionActive
                                    text: (modelData.health.exceptionAcknowledged ? "✓ ack: " : "⚠ ")
                                          + modelData.health.exceptionReason
                                    color: modelData.health.exceptionAcknowledged ? "#8a93a3" : "#e05a4e"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight; width: parent.width
                                }
                            }

                            // per-device actions
                            Row {
                                id: actions
                                anchors.right: parent.right
                                anchors.rightMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 8

                                PillButton {
                                    visible: modelData.health.exceptionActive
                                             && !modelData.health.exceptionAcknowledged
                                    label: "Acknowledge"; tone: "#f2a33c"
                                    onClicked: devicesCtrl.acknowledge(modelData.id)
                                }
                                PillButton {
                                    label: modelData.health.inMaintenance
                                            ? "End maint." : "Maintenance"
                                    tone: "#3a6ea5"
                                    onClicked: devicesCtrl.setMaintenance(
                                        modelData.id, !modelData.health.inMaintenance)
                                }
                                PillButton {
                                    label: "Remove"; tone: "#e05a4e"
                                    onClicked: devicesCtrl.removeDevice(modelData.id)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
