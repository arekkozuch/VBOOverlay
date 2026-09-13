pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "comparisonLapDialog"
    title: qsTr("Compare laps · A / B")
    modal: true
    anchors.centerIn: parent
    width: Math.min(820, parent.width - 32)
    height: Math.min(550, parent.height - 24)
    standardButtons: Dialog.Close
    property string actionError: ""
    readonly property var slots: appController.comparisonSlots
    readonly property var laps: appController.comparisonLaps
    onOpened: actionError = ""
    function result(ok) {
        actionError = ok ? "" : qsTr("Could not select this lap. Check its availability and track configuration.");
    }
    contentItem: ScrollView {
        id: scroll
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: scroll.availableWidth
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: qsTr("Choose eligible laps from matching track configurations. Each selection loads independently.")
                wrapMode: Text.WordWrap
                color: "#91a0b2"
            }
            Repeater {
                model: 2
                delegate: ColumnLayout {
                    id: entry
                    required property int index
                    readonly property var slot: root.slots[index]
                    readonly property var other: root.slots[1 - index]
                    readonly property var choices: root.laps.filter(lap =>
                        !["ready", "loading"].includes(other.state)
                        || lap.compatibilityGroupId === other.lap.compatibilityGroupId)
                    Layout.fillWidth: true
                    spacing: 5
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: entry.index === 0 ? qsTr("Lap A") : qsTr("Lap B")
                            color: entry.index === 0 ? "#55e6a5" : "#58bfff"
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        FeButton {
                            objectName: "inspectComparison" + entry.index
                            text: qsTr("Inspect")
                            compact: true
                            enabled: entry.slot.state === "ready" && !appController.outingLapsLoading
                            onClicked: {
                                const ok = appController.inspectComparisonLap(entry.index);
                                root.result(ok);
                                if (ok) root.close();
                            }
                        }
                        FeButton {
                            objectName: "clearComparison" + entry.index
                            text: qsTr("Clear")
                            compact: true
                            enabled: entry.slot.state !== "empty"
                            onClicked: { appController.clearComparisonLap(entry.index); root.actionError = ""; }
                        }
                    }
                    FeComboBox {
                        objectName: "comparisonLapPicker" + entry.index
                        Layout.fillWidth: true
                        model: entry.choices.map(lap => lap.label)
                        popupMinimumWidth: 420
                        wrapPopupText: true
                        currentIndex: entry.choices.findIndex(lap => JSON.stringify(lap.reference) === JSON.stringify(entry.slot.lap.reference))
                        displayText: currentIndex >= 0 ? entry.choices[currentIndex].label
                            : entry.slot.lap.runName ? entry.slot.lap.runName + " · LAP " + entry.slot.lap.lapNumber
                            : qsTr("Choose a lap…")
                        enabled: !appController.outingLapsLoading && count > 0
                        onActivated: function(index) {
                            if (index >= 0 && index < entry.choices.length)
                                root.result(appController.selectComparisonLap(entry.index, entry.choices[index].reference));
                        }
                    }
                    Label {
                        objectName: "comparisonLapState" + entry.index
                        Layout.fillWidth: true
                        text: entry.slot.state === "loading" ? qsTr("Loading recording…")
                            : entry.slot.state === "error" ? entry.slot.error
                            : entry.slot.state === "ready" ? qsTr("Ready · %1").arg(entry.slot.lap.compatibilityGroupLabel || "")
                            : qsTr("No lap selected")
                        color: entry.slot.state === "error" ? "#ffb84d" : "#91a0b2"
                        wrapMode: Text.WordWrap
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                FeButton {
                    objectName: "swapComparisonLaps"
                    text: qsTr("Swap A / B")
                    compact: true
                    enabled: !!root.slots[0].lap.reference && !!root.slots[1].lap.reference
                    onClicked: root.result(appController.swapComparisonLaps())
                }
                FeButton {
                    objectName: "bestRunAsComparisonB"
                    text: qsTr("Best of A’s run → B")
                    compact: true
                    enabled: !!root.slots[0].lap.reference && !appController.outingLapsLoading
                    onClicked: root.result(appController.useBestComparisonLap(false))
                }
                FeButton {
                    objectName: "bestDayAsComparisonB"
                    text: qsTr("Best of group → B")
                    compact: true
                    enabled: root.laps.length > 0 && !appController.outingLapsLoading
                    onClicked: root.result(appController.useBestComparisonLap(true))
                }
            }
            Label {
                objectName: "comparisonPairStatus"
                Layout.fillWidth: true
                text: root.actionError || (appController.comparisonPairReady
                    ? qsTr("A and B are ready. Inspect either lap to review its channels and map.")
                    : qsTr("Select two available, compatible laps."))
                color: root.actionError ? "#ffb84d" : "#91a0b2"
                wrapMode: Text.WordWrap
            }
        }
    }
}
