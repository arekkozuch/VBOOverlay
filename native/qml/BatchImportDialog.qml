pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "batchImportDialog"
    title: qsTr("Import runs")
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: Math.min(parent ? parent.width - 32 : 840, 840)
    height: Math.min(parent ? parent.height - 32 : 650, 650)
    anchors.centerIn: parent
    property var assignments: ({})
    property bool appendToEvent: destination.currentIndex === 1
    property bool reviewing: appController.batchImportState === "review"
    property var proposals: appController.batchImportRows.filter(row => row.status === "ready")
    property bool showDetails: false
    function duration(seconds) {
        const total = Math.max(0, Math.round(Number(seconds || 0)));
        return Math.floor(total / 60) + ":" + String(total % 60).padStart(2, "0");
    }

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
        showDetails = false;
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
            text: qsTr("Import your runs into one track day. Each file becomes a separate run. For RaceChrono’s calculated lateral and longitudinal G, choose VBO. RCZ keeps the original recording rates.")
        }
        RowLayout {
            Layout.fillWidth: true
            FeComboBox {
                id: destination
                Layout.preferredWidth: 225
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
                : qsTr("%1 files · %2 ready to import").arg(appController.batchImportRows.length).arg(root.proposals.length)
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
                                text: (rowFrame.index + 1) + ". " + rowFrame.modelData.name
                                font.bold: true
                                elide: Text.ElideMiddle
                            }
                            Label {
                                Layout.fillWidth: true
                                text: rowFrame.modelData.path
                                visible: root.showDetails
                                elide: Text.ElideMiddle
                                font.pixelSize: 10
                                HoverHandler { id: pathHover }
                                ToolTip.visible: pathHover.hovered
                                ToolTip.text: text
                            }
                            Label {
                                visible: rowFrame.modelData.status === "ready"
                                text: qsTr("%1 · %2 complete laps")
                                    .arg(root.duration(rowFrame.modelData.duration))
                                    .arg(rowFrame.modelData.laps || 0)
                            }
                            FeComboBox {
                                id: grouping
                                Layout.fillWidth: true
                                visible: rowFrame.modelData.status === "ready"
                                enabled: root.reviewing && !(root.appendToEvent && rowFrame.modelData.existing)
                                model: [qsTr("Import as a run"), qsTr("Skip this file")]
                                    .concat(rowFrame.targets.map(row => qsTr("Same run as: %1").arg(row.name)))
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
                                visible: text.length > 0 && (root.showDetails || rowFrame.modelData.status !== "ready"
                                    || (root.appendToEvent && rowFrame.modelData.existing))
                                text: rowFrame.modelData.message || ""
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
        CheckBox {
            text: qsTr("Show file details and import warnings")
            checked: root.showDetails
            onToggled: root.showDetails = checked
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Two exports of the same run? Choose ‘Same run as’. Analysis uses the file you link to; the other file is kept with that run.")
            color: "#aab6c4"
            font.pixelSize: 11
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
