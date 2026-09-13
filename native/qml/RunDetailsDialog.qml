pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "runDetailsDialog"
    title: qsTr("Run details")
    modal: true
    anchors.centerIn: parent
    width: Math.min(560, parent.width - 40)
    height: Math.min(600, parent.height - 30)
    closePolicy: Popup.CloseOnEscape
    property string editingRunId: ""
    property var captured: ({})
    readonly property bool draftChanged: nameField.text !== (captured.name || "")
        || notesField.text !== (captured.notes || "")
        || conditionsField.text !== (captured.conditions || "")
        || setupField.text !== (captured.setupChanges || "")
    readonly property bool validDraft: nameField.text.trim().length > 0 && nameField.text.length <= 160
        && notesField.text.length <= 4096 && conditionsField.text.length <= 4096 && setupField.text.length <= 4096
        && [nameField.text, notesField.text, conditionsField.text, setupField.text].every(text => text.indexOf("\u0000") < 0)
    function loadRun(runId) {
        editingRunId = runId;
        captured = appController.runMetadata(runId);
        nameField.text = captured.name || "";
        notesField.text = captured.notes || "";
        conditionsField.text = captured.conditions || "";
        setupField.text = captured.setupChanges || "";
        errorLabel.text = "";
    }
    onOpened: {
        runPicker.currentIndex = runPicker.indexOfValue(appController.activeRunId);
        if (runPicker.currentIndex < 0 && runPicker.count > 0) runPicker.currentIndex = 0;
        loadRun(runPicker.currentValue || "");
        nameField.forceActiveFocus();
    }
    contentItem: Item {
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            ComboBox {
                id: runPicker
                objectName: "runDetailsPicker"
                Layout.fillWidth: true
                model: appController.eventRuns
                textRole: "name"
                valueRole: "id"
                enabled: !root.draftChanged
                Accessible.name: qsTr("Run to edit")
                onActivated: root.loadRun(currentValue)
            }
            Label {
                Layout.fillWidth: true
                text: root.draftChanged ? qsTr("Save or cancel your edits before choosing another run.")
                    : qsTr("Leave unknown details blank. These notes do not change lap timing.")
                wrapMode: Text.WordWrap
                color: "#91a0b2"
            }
            ScrollView {
                id: fieldsScroll
                objectName: "runDetailsScroll"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 80
                Layout.preferredHeight: 240
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ColumnLayout {
                    width: fieldsScroll.availableWidth
                    spacing: 8
                    Label { text: qsTr("Run name · required · %1/160").arg(nameField.text.length) }
                    TextField {
                        id: nameField
                        objectName: "runDetailsName"
                        Layout.fillWidth: true
                        maximumLength: 160
                        Accessible.name: qsTr("Run name")
                    }
                    Label { text: qsTr("Notes · %1/4096").arg(notesField.text.length); color: notesField.text.length > 4096 ? "#ff9585" : "#dce4ee" }
                    TextArea {
                        id: notesField
                        objectName: "runDetailsNotes"
                        Layout.fillWidth: true
                        Layout.minimumHeight: 90
                        wrapMode: TextEdit.Wrap
                        placeholderText: qsTr("No notes")
                        Accessible.name: qsTr("Run notes")
                        textFormat: TextEdit.PlainText
                    }
                    Label { text: qsTr("Conditions · %1/4096").arg(conditionsField.text.length); color: conditionsField.text.length > 4096 ? "#ff9585" : "#dce4ee" }
                    TextArea {
                        id: conditionsField
                        objectName: "runDetailsConditions"
                        Layout.fillWidth: true
                        Layout.minimumHeight: 90
                        wrapMode: TextEdit.Wrap
                        placeholderText: qsTr("Unknown — enter observed weather or track conditions")
                        Accessible.name: qsTr("Conditions")
                        textFormat: TextEdit.PlainText
                    }
                    Label { text: qsTr("Setup changes · %1/4096").arg(setupField.text.length); color: setupField.text.length > 4096 ? "#ff9585" : "#dce4ee" }
                    TextArea {
                        id: setupField
                        objectName: "runDetailsSetup"
                        Layout.fillWidth: true
                        Layout.minimumHeight: 90
                        wrapMode: TextEdit.Wrap
                        placeholderText: qsTr("Unknown — enter changes made for this run")
                        Accessible.name: qsTr("Setup changes")
                        textFormat: TextEdit.PlainText
                    }
                }
            }
            Label {
                id: errorLabel
                objectName: "runDetailsError"
                Layout.fillWidth: true
                visible: text.length > 0
                wrapMode: Text.WordWrap
                color: "#ff9585"
            }
        }
    }
    footer: DialogButtonBox {
        FeButton {
            objectName: "cancelRunDetails"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            text: qsTr("Cancel")
            onClicked: root.close()
        }
        FeButton {
            objectName: "saveRunDetails"
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            text: qsTr("Save details")
            enabled: root.validDraft && !!root.captured.editToken && !appController.projectLoading
            onClicked: {
                if (appController.updateRunMetadata(root.editingRunId, root.captured.editToken || "",
                    nameField.text, notesField.text, conditionsField.text, setupField.text)) root.close();
                else errorLabel.text = qsTr("Could not save these details. Cancel and reopen to review the current run before editing again.");
            }
        }
    }
}
