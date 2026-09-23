pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// F1-debrief-style A/B lap comparison: both laps overlaid on one chart
// (distance into lap, not raw time, so a corner lines up for both) and one
// track map (shared scale). Drag to zoom into a corner; hover shows both
// laps' values and where each car was on track at that point.
Rectangle {
    id: root
    color: "#090e14"
    readonly property var slots: appController.comparisonSlots
    readonly property var availableChannels: appController.comparisonAvailableChannels
    property string selectedChannel: "speed"
    onAvailableChannelsChanged: {
        if (availableChannels.length > 0 && availableChannels.indexOf(selectedChannel) < 0)
            selectedChannel = availableChannels.indexOf("speed") >= 0 ? "speed" : availableChannels[0];
    }
    function duration(seconds) {
        return Math.floor(seconds / 60) + ":" + (seconds % 60).toFixed(3).padStart(6, "0");
    }
    function lapLabel(index) {
        const lap = root.slots[index] ? root.slots[index].lap : ({});
        return (index === 0 ? qsTr("Lap A · ") : qsTr("Lap B · ")) + (lap.runName || "")
            + " · LAP " + (lap.lapNumber || "") + " · " + root.duration(Number(lap.durationSeconds || 0));
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
        ColumnLayout {
            visible: appController.comparisonPairReady
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                spacing: 20
                Label {
                    objectName: "comparisonLapLabelA"
                    text: root.lapLabel(0)
                    color: "#55e6a5"
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                }
                Label {
                    objectName: "comparisonLapLabelB"
                    text: root.lapLabel(1)
                    color: "#58bfff"
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: qsTr("Channel")
                    color: "#8d9aaa"
                    font.pixelSize: 11
                }
                FeComboBox {
                    id: channelPicker
                    objectName: "comparisonChannelPicker"
                    implicitHeight: 28
                    implicitWidth: 160
                    model: root.availableChannels
                    currentIndex: model.indexOf(root.selectedChannel)
                    onActivated: index => root.selectedChannel = model[index]
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                ComparisonOverlayMap {
                    id: overlayMap
                    Layout.preferredWidth: Math.max(160, root.width * 0.28)
                    Layout.fillHeight: true
                    hoverDistanceMeters: overlayChart.hoverDistanceMeters
                }
                ComparisonOverlayChart {
                    id: overlayChart
                    objectName: "comparisonOverlayChart"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    channel: root.selectedChannel
                }
            }
        }
    }
}
