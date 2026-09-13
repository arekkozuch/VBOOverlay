pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#090e14"
    function duration(seconds) {
        const minutes = Math.floor(seconds / 60);
        return minutes + ":" + (seconds - minutes * 60).toFixed(3).padStart(6, "0");
    }
    Dialog {
        id: configurationDialog
        objectName: "outingTrackConfigurationDialog"
        title: qsTr("Track configuration")
        modal: true
        anchors.centerIn: parent
        width: Math.min(560, root.width - 40)
        property string editingRunId: ""
        property var capturedConfiguration: ({})
        function loadConfiguration() {
            editingRunId = runPicker.currentValue || "";
            capturedConfiguration = appController.runTrackConfiguration(editingRunId);
            layoutName.text = capturedConfiguration.layoutId || "";
            directionPicker.currentIndex = capturedConfiguration.direction === "clockwise" ? 1
                : capturedConfiguration.direction === "counterclockwise" ? 2 : 0;
            configurationError.text = "";
        }
        onOpened: loadConfiguration()
        contentItem: ColumnLayout {
            spacing: 12
            Label { text: qsTr("Recording"); color: "#91a0b2" }
            ComboBox {
                id: runPicker
                objectName: "compatibilityRunPicker"
                Layout.fillWidth: true
                model: appController.eventRuns
                textRole: "name"
                valueRole: "id"
                onActivated: configurationDialog.loadConfiguration()
                Accessible.name: qsTr("Recording configuration")
            }
            Label { text: qsTr("Layout name — use the same name for the same layout"); color: "#91a0b2" }
            TextField {
                id: layoutName
                objectName: "compatibilityLayoutName"
                Layout.fillWidth: true
                maximumLength: 128
                placeholderText: qsTr("e.g. Jastrząb full circuit")
                Accessible.name: qsTr("Track layout name")
            }
            ComboBox {
                id: directionPicker
                objectName: "compatibilityDirectionPicker"
                Layout.fillWidth: true
                model: [qsTr("Direction unknown"), qsTr("Clockwise"), qsTr("Counterclockwise")]
                Accessible.name: qsTr("Driving direction")
            }
            Label {
                Layout.fillWidth: true
                text: configurationDialog.capturedConfiguration.gateRevision
                    ? qsTr("Timing gates are recorded in this source; differing gates stay in separate groups.")
                    : qsTr("Timing gates are unresolved. This recording cannot join a comparison group until verified source gates are available.")
                wrapMode: Text.WordWrap
                color: "#d6a457"
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Applies to all laps in this recording. Changing configuration may leave saved lap exclusions unmatched.")
                wrapMode: Text.WordWrap
                color: "#91a0b2"
            }
            Label {
                id: configurationError
                Layout.fillWidth: true
                visible: text.length > 0
                wrapMode: Text.WordWrap
                color: "#ffb84d"
            }
            RowLayout {
                Layout.fillWidth: true
                FeButton {
                    text: qsTr("Cancel")
                    onClicked: configurationDialog.close()
                }
                Item { Layout.fillWidth: true }
                FeButton {
                    objectName: "confirmTrackConfiguration"
                    text: qsTr("Confirm configuration")
                    enabled: layoutName.text.trim().length > 0 && directionPicker.currentIndex > 0
                    onClicked: {
                        if (appController.confirmRunTrackConfiguration(configurationDialog.editingRunId,
                            configurationDialog.capturedConfiguration.derivationKey || "", layoutName.text,
                            directionPicker.currentIndex === 1 ? "clockwise" : "counterclockwise")) configurationDialog.close();
                        else configurationError.text = qsTr("Could not save this configuration. Reopen the dialog to review the current recording.");
                    }
                }
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("All laps"); color: "#f2f6fb"; font.pixelSize: 24; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            Label {
                text: qsTr("%1 runs · %2 recorded sections").arg(appController.eventRuns.length).arg(appController.outingLaps.length)
                color: "#91a0b2"
                font.pixelSize: 12
            }
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Click a row to open it. Chronological order · UTC. OUT before the first start/finish crossing; IN after the last. Entries without a reliable clock or crossing are marked.")
            wrapMode: Text.WordWrap
            color: "#657386"
            font.pixelSize: 11
        }
        RowLayout {
            Layout.fillWidth: true
            ComboBox {
                id: comparisonGroup
                objectName: "outingComparisonGroupPicker"
                Layout.fillWidth: true
                model: appController.outingCompatibilityGroups.filter(group => group.resolved)
                textRole: "summary"
                valueRole: "id"
                currentIndex: { const count = model.length; return indexOfValue(appController.outingComparisonGroupId); }
                displayText: currentIndex < 0 ? qsTr("Choose a comparison group") : currentText
                onActivated: appController.selectOutingComparisonGroup(currentValue)
                Accessible.name: qsTr("Comparison group")
            }
            FeButton {
                objectName: "openTrackConfiguration"
                text: qsTr("Track configuration…")
                enabled: appController.eventRuns.length > 0 && !appController.projectLoading
                onClicked: configurationDialog.open()
            }
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("%1 compatible groups · %2 unresolved recordings. Confirm layout and direction to resolve compatibility; all laps stay visible.")
                .arg(appController.outingCompatibilityGroups.filter(group => group.resolved).length)
                .arg(appController.outingCompatibilityGroups.filter(group => !group.resolved).length)
            wrapMode: Text.WordWrap
            color: "#91a0b2"
            font.pixelSize: 11
        }
        BusyIndicator { running: appController.outingLapsLoading; visible: running; Layout.alignment: Qt.AlignHCenter }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(80, notices.implicitHeight)
            visible: appController.outingLapMessages.length > 0
            contentWidth: availableWidth
            Label {
                id: notices
                width: parent.width
                text: appController.outingLapMessages.join("\n")
                color: "#d6a457"
                wrapMode: Text.WordWrap
                font.pixelSize: 11
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: "#101720"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12; anchors.rightMargin: 12
                spacing: 12
                Label { text: "#"; Layout.preferredWidth: 32; color: "#718092" }
                Label { text: qsTr("Start · UTC"); Layout.preferredWidth: 176; color: "#718092" }
                Label { text: qsTr("Type"); Layout.preferredWidth: 76; color: "#718092" }
                Label { text: qsTr("Run"); Layout.fillWidth: true; color: "#718092" }
                Label { text: qsTr("Duration"); Layout.preferredWidth: 92; horizontalAlignment: Text.AlignRight; color: "#718092" }
            }
        }
        ListView {
            id: laps
            objectName: "outingLapList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: appController.outingLaps
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: Rectangle {
                id: row
                required property var modelData
                required property int index
                width: laps.width
                height: 66
                radius: 6
                objectName: "outingLapRow" + index
                activeFocusOnTab: true
                color: pointer.containsMouse || activeFocus ? "#193529" : index % 2 ? "#0c131b" : "#101923"
                border.color: activeFocus ? "#55e6a5" : "transparent"
                Accessible.role: Accessible.Button
                Accessible.name: modelData.type + " " + modelData.lapNumber + " · " + modelData.runName
                Accessible.description: (modelData.compatibilityReasonLabels || []).join("; ")
                    + (modelData.exclusionReason ? "; " + modelData.exclusionReason : "")
                Accessible.onPressAction: appController.selectOutingLapReference(row.modelData.reference)
                Keys.onReturnPressed: appController.selectOutingLapReference(row.modelData.reference)
                Keys.onEnterPressed: appController.selectOutingLapReference(row.modelData.reference)
                Keys.onSpacePressed: appController.selectOutingLapReference(row.modelData.reference)
                MouseArea {
                    id: pointer
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { row.forceActiveFocus(); appController.selectOutingLapReference(row.modelData.reference); }
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12; anchors.rightMargin: 12
                    spacing: 12
                    Label { text: row.index + 1; Layout.preferredWidth: 32; color: "#657386"; font.pixelSize: 11 }
                    Label { text: row.modelData.clock; Layout.preferredWidth: 176; color: "#aab6c4"; font.pixelSize: 11; elide: Text.ElideRight }
                    Label {
                        text: row.modelData.type === "LAP" ? "LAP " + row.modelData.lapNumber : row.modelData.type
                        Layout.preferredWidth: 76
                        color: row.modelData.type === "LAP" ? "#55e6a5" : row.modelData.type === "UNKNOWN" ? "#d6a457" : "#74a9d8"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Label {
                            text: row.modelData.runName
                                + (row.modelData.excluded ? qsTr(" · Excluded: ") + row.modelData.exclusionReason : "")
                                + (row.modelData.referenceIssue ? " · " + row.modelData.referenceIssue : "")
                                + (row.modelData.bestOfRun ? qsTr(" · Best of run") : "")
                            Layout.fillWidth: true
                            color: "#dce4ee"
                            font.pixelSize: 12
                            elide: Text.ElideMiddle
                        }
                        Label {
                            text: (row.modelData.compatibilityGroupLabel || "") + " · "
                                + ((row.modelData.compatibilityReasonLabels || []).join("; ")
                                    || (row.modelData.comparisonEligible ? qsTr("Eligible in selected group") : qsTr("Choose a group to compare")))
                            Layout.fillWidth: true
                            color: row.modelData.comparisonEligible ? "#55e6a5" : "#d6a457"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                            ToolTip.visible: pointer.containsMouse
                            ToolTip.text: text
                        }
                    }
                    Label { text: root.duration(row.modelData.durationSeconds); Layout.preferredWidth: 92; horizontalAlignment: Text.AlignRight; color: "#f2f6fb"; font.family: "Menlo"; font.pixelSize: 12 }
                }
            }
            Label {
                anchors.centerIn: parent
                visible: laps.count === 0 && !appController.outingLapsLoading
                text: qsTr("No recorded lap sections available")
                color: "#657386"
            }
        }
    }
}
