pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#090e14"
    readonly property var lap: appController.selectedOutingLap
    readonly property bool ready: appController.outingLapDetailState === "ready"
    function duration(seconds) {
        return Math.floor(seconds / 60) + ":" + (seconds % 60).toFixed(3).padStart(6, "0");
    }
    Shortcut { sequence: "Escape"; enabled: root.visible; onActivated: appController.closeOutingLap() }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            FeButton {
                objectName: "backToOutingLaps"
                text: qsTr("← All laps")
                onClicked: appController.closeOutingLap()
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Label {
                    text: (root.lap.type || "") + (root.lap.type === "LAP" ? " " + root.lap.lapNumber : "")
                        + " · " + root.duration(Number(root.lap.durationSeconds || 0))
                    color: "#55e6a5"
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                }
                Label {
                    text: root.lap.runName || ""
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                    color: "#91a0b2"
                    font.pixelSize: 11
                }
            }
        }
        RowLayout {
            visible: root.ready && root.lap.type === "LAP"
            Layout.fillWidth: true
            Label {
                text: root.lap.excluded ? qsTr("Excluded from comparisons") : qsTr("Lap eligibility")
                color: root.lap.excluded ? "#ffb84d" : "#91a0b2"
            }
            TextField {
                id: exclusionReason
                objectName: "lapExclusionReason"
                Layout.fillWidth: true
                maximumLength: 256
                placeholderText: qsTr("Reason, e.g. traffic or cooldown")
                text: root.lap.exclusionReason || ""
                readOnly: Boolean(root.lap.excluded)
                Accessible.name: qsTr("Lap exclusion reason")
            }
            FeButton {
                objectName: "toggleLapExclusion"
                text: root.lap.excluded ? qsTr("Restore lap") : qsTr("Exclude lap")
                enabled: Boolean(root.lap.excluded) || exclusionReason.text.trim().length > 0
                onClicked: {
                    if (!appController.setOutingLapExcluded(root.lap.reference, !root.lap.excluded, exclusionReason.text))
                        exclusionError.text = qsTr("Could not change this lap. Reopen the lap and try again.");
                    else exclusionError.text = "";
                }
            }
        }
        Label {
            id: exclusionError
            Layout.fillWidth: true
            visible: text.length > 0
            color: "#ffb84d"
            wrapMode: Text.WordWrap
        }
        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: appController.outingLapDetailState === "loading"
            visible: running
        }
        Label {
            Layout.fillWidth: true
            visible: appController.outingLapDetailState === "error"
            text: appController.outingLapDetailError
            color: "#ffb84d"
            wrapMode: Text.WordWrap
        }
        RowLayout {
            visible: root.ready
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10
            TrackMapPanel {
                lapDetail: true
                Layout.preferredWidth: Math.max(190, root.width * 0.29)
                Layout.fillHeight: true
            }
            AnalysisPanel {
                objectName: "outingLapCharts"
                lapDetail: true
                mediaDuration: 0
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
        RowLayout {
            visible: root.ready
            Layout.fillWidth: true
            Label {
                text: qsTr("Section time")
                color: "#91a0b2"
            }
            Slider {
                objectName: "outingLapCursorSlider"
                Layout.fillWidth: true
                from: Number(root.lap.startTime || 0)
                to: Number(root.lap.endTime || 1)
                value: appController.outingLapCursor
                onMoved: appController.outingLapCursor = value
            }
            Label {
                text: root.duration(Math.max(0, appController.outingLapCursor - Number(root.lap.startTime || 0)))
                color: "#f2f6fb"
                font.family: "Menlo"
                Layout.preferredWidth: 94
                horizontalAlignment: Text.AlignRight
            }
        }
        Item { visible: !root.ready; Layout.fillHeight: true }
    }
}
