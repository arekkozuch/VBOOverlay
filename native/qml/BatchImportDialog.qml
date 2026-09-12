pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "batchImportDialog"
    title: qsTr("Import telemetry runs")
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: Math.min(parent ? parent.width - 32 : 840, 840)
    height: Math.min(parent ? parent.height - 32 : 650, 650)
    anchors.centerIn: parent
    property var assignments: ({})
    property bool appendToEvent: destination.currentIndex === 1
    property bool reviewing: appController.batchImportState === "review"
    property var proposals: appController.batchImportRows.filter(row => row.status === "ready")

    function choices() {
        return proposals.map(row => ({proposalId: row.proposalId,
            groupId: appendToEvent && row.existing ? ""
                : assignments[row.proposalId] === undefined ? row.proposalId : assignments[row.proposalId]}));
    }
    function submit() {
        appController.confirmBatchImport(eventName.text, appendToEvent, choices());
    }
    onOpened: {
        assignments = ({});
        destination.currentIndex = appController.eventRuns.length > 0 ? 1 : 0;
        eventName.text = "";
    }
    onAboutToHide: appController.cancelBatchImport()
    Connections {
        target: appController
        function onBatchImportCommitted() { root.close(); }
    }
    contentItem: ColumnLayout {
        spacing: 10
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Review before changing the project. Files stay separate unless you explicitly group them. The selected primary supplies telemetry; alternatives are retained without channel fusion.")
        }
        RowLayout {
            Layout.fillWidth: true
            ComboBox {
                id: destination
                objectName: "batchDestination"
                model: appController.eventRuns.length > 0
                    ? [qsTr("Create a new event"), qsTr("Add runs to current event")]
                    : [qsTr("Create a new event")]
                enabled: root.reviewing
                onCurrentIndexChanged: root.assignments = ({})
            }
            TextField {
                id: eventName
                objectName: "batchEventName"
                Layout.fillWidth: true
                maximumLength: 160
                placeholderText: root.appendToEvent ? appController.eventName : qsTr("Event name (required)")
                enabled: root.reviewing && !root.appendToEvent
            }
        }
        Label {
            Layout.fillWidth: true
            visible: !root.appendToEvent && appController.dirty
            text: qsTr("Save your current edits before creating a new event. Cancel this review, save, then import again; or append to the current event.")
            wrapMode: Text.WordWrap
            color: "#e5a54b"
        }
        ProgressBar {
            Layout.fillWidth: true
            visible: !root.reviewing && appController.batchImportState !== "error"
            from: 0
            to: Math.max(1, appController.batchImportTotal)
            value: appController.batchImportProcessed
        }
        Label {
            text: appController.batchImportState === "preparing"
                ? qsTr("Preparing review · %1/%2 files parsed").arg(appController.batchImportProcessed).arg(appController.batchImportTotal)
                : appController.batchImportState === "validating" ? qsTr("Rechecking selected files before commit…")
                : appController.batchImportState === "cancelling" ? qsTr("Cancelling…")
                : qsTr("%1 file results · %2 sources available for review").arg(appController.batchImportRows.length).arg(root.proposals.length)
        }
        ScrollView {
            id: results
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: results.availableWidth
                spacing: 8
                Repeater {
                    model: appController.batchImportRows
                    delegate: Frame {
                        id: rowFrame
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        property var targets: root.proposals.filter(row => row.proposalId !== rowFrame.modelData.proposalId
                            && !(root.appendToEvent && row.existing))
                        contentItem: ColumnLayout {
                            Label {
                                Layout.fillWidth: true
                                text: (rowFrame.index + 1) + ". " + rowFrame.modelData.name + " — " + rowFrame.modelData.status
                                font.bold: true
                                elide: Text.ElideMiddle
                            }
                            Label {
                                Layout.fillWidth: true
                                text: rowFrame.modelData.path
                                elide: Text.ElideMiddle
                                font.pixelSize: 10
                                HoverHandler { id: pathHover }
                                ToolTip.visible: pathHover.hovered
                                ToolTip.text: text
                            }
                            Label {
                                visible: rowFrame.modelData.status === "ready"
                                text: qsTr("%1 s · %2 complete laps · %3 channels")
                                    .arg(Number(rowFrame.modelData.duration || 0).toFixed(1))
                                    .arg(rowFrame.modelData.laps || 0).arg(rowFrame.modelData.channels || 0)
                            }
                            ComboBox {
                                id: grouping
                                Layout.fillWidth: true
                                visible: rowFrame.modelData.status === "ready"
                                enabled: root.reviewing && !(root.appendToEvent && rowFrame.modelData.existing)
                                model: [qsTr("Separate run — use this file as primary"), qsTr("Skip this source")]
                                    .concat(rowFrame.targets.map((row, i) => qsTr("Alternative of: %1 [%2]").arg(row.name).arg(i + 1)))
                                currentIndex: {
                                    const selected = root.appendToEvent && rowFrame.modelData.existing ? "" : root.assignments[rowFrame.modelData.proposalId];
                                    if (selected === undefined || selected === rowFrame.modelData.proposalId) return 0;
                                    if (selected === "") return 1;
                                    const found = rowFrame.targets.findIndex(row => row.proposalId === selected);
                                    return found < 0 ? 0 : found + 2;
                                }
                                onActivated: index => {
                                    const next = Object.assign({}, root.assignments);
                                    next[rowFrame.modelData.proposalId] = index === 0 ? rowFrame.modelData.proposalId
                                        : index === 1 ? "" : rowFrame.targets[index - 2].proposalId;
                                    root.assignments = next;
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: rowFrame.modelData.message || ""
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: appController.batchImportError
            wrapMode: Text.WordWrap
            color: "#df724f"
        }
    }
    footer: DialogButtonBox {
        Button {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: root.close()
        }
        Button {
            objectName: "batchConfirm"
            text: root.appendToEvent ? qsTr("Add reviewed runs") : qsTr("Create event")
            enabled: root.reviewing && root.proposals.length > 0
                && (root.appendToEvent || (!appController.dirty && eventName.text.trim().length > 0))
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: root.submit()
        }
    }
}
