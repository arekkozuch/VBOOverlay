pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Side-by-side A/B lap inspection. Each side reuses the single-lap TrackMapPanel
// and AnalysisPanel components via their comparisonSlot property; there is no
// shared track-progress axis, delta, or synced cursor yet (each side scrubs
// independently once that lands).
Rectangle {
    id: root
    color: "#090e14"
    readonly property var slots: appController.comparisonSlots
    function duration(seconds) {
        return Math.floor(seconds / 60) + ":" + (seconds % 60).toFixed(3).padStart(6, "0");
    }
    Shortcut { sequence: "Escape"; enabled: root.visible; onActivated: appController.comparisonViewOpen = false }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            FeButton {
                objectName: "backFromComparison"
                text: qsTr("← All laps")
                onClicked: appController.comparisonViewOpen = false
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Compare laps · A / B")
                color: "#55e6a5"
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }
        }
        Label {
            Layout.fillWidth: true
            visible: !appController.comparisonPairReady
            text: qsTr("Select two ready, compatible laps to compare.")
            color: "#91a0b2"
            wrapMode: Text.WordWrap
        }
        RowLayout {
            visible: appController.comparisonPairReady
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14
            Repeater {
                objectName: "comparisonDetailColumns"
                model: 2
                delegate: ColumnLayout {
                    id: column
                    required property int index
                    readonly property var lap: root.slots[index] ? root.slots[index].lap : ({})
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                        text: (column.index === 0 ? qsTr("Lap A · ") : qsTr("Lap B · "))
                            + (column.lap.runName || "") + " · LAP " + (column.lap.lapNumber || "")
                            + " · " + root.duration(Number(column.lap.durationSeconds || 0))
                        color: column.index === 0 ? "#55e6a5" : "#58bfff"
                        font.pixelSize: 13
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8
                        TrackMapPanel {
                            comparisonSlot: column.index
                            Layout.preferredWidth: Math.max(150, column.width * 0.34)
                            Layout.fillHeight: true
                        }
                        AnalysisPanel {
                            objectName: "comparisonCharts" + column.index
                            comparisonSlot: column.index
                            mediaDuration: 0
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                    }
                }
            }
        }
    }
}
