pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// F1-debrief-style A/B lap comparison: both laps overlaid on shared charts
// (distance into lap, not raw time, so a corner lines up for both) and one
// track map (shared scale), with a shared zoom/pan window so every channel
// row and the map move together. Scroll to zoom (centered under the
// cursor), scroll sideways to pan, or drag to select a section; hover shows
// both laps' values, their delta, and where each car was on track.
Rectangle {
    id: root
    color: "#090e14"
    readonly property var slots: appController.comparisonSlots
    readonly property var availableChannels: appController.comparisonAvailableChannels
    property var visibleChannels: []
    // comparisonLapDistanceTotal is a Q_INVOKABLE, not a property: QML's
    // automatic dependency tracking only follows real property reads, so
    // force one on comparisonSlots or these never update once a lap loads.
    readonly property real totalMetersA: (appController.comparisonSlots, appController.comparisonLapDistanceTotal(0))
    readonly property real totalMetersB: (appController.comparisonSlots, appController.comparisonLapDistanceTotal(1))
    readonly property real totalMeters: Math.max(1, totalMetersA, totalMetersB)
    property real zoomStart: 0
    property real zoomEnd: totalMeters
    property real hoverDistanceMeters: -1
    readonly property bool zoomed: zoomStart > 1e-3 || zoomEnd < totalMeters - 1e-3
    onTotalMetersChanged: { zoomStart = 0; zoomEnd = totalMeters; }

    function defaultChannels(available) {
        const preferred = ["speed", "throttle", "brake"];
        const picked = preferred.filter(channel => available.indexOf(channel) >= 0);
        return picked.length > 0 ? picked.slice(0, 4) : available.slice(0, Math.min(2, available.length));
    }
    onAvailableChannelsChanged: {
        const stillValid = root.visibleChannels.filter(channel => root.availableChannels.indexOf(channel) >= 0);
        root.visibleChannels = stillValid.length > 0 ? stillValid : root.defaultChannels(root.availableChannels);
    }
    function toggleChannel(channel) {
        const channels = root.visibleChannels.slice();
        const index = channels.indexOf(channel);
        if (index >= 0) channels.splice(index, 1);
        else if (channel && channels.length < 4) channels.push(channel);
        root.visibleChannels = channels;
    }
    function replaceChannel(previous, next) {
        const channels = root.visibleChannels.slice();
        const index = channels.indexOf(previous);
        if (index >= 0 && (previous === next || channels.indexOf(next) < 0)) {
            channels[index] = next;
            root.visibleChannels = channels;
        }
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
        spacing: 10
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
            spacing: 6
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
                FeButton {
                    objectName: "comparisonResetZoom"
                    visible: root.zoomed
                    compact: true
                    text: qsTr("Reset zoom (%1 m)").arg((root.zoomEnd - root.zoomStart).toFixed(0))
                    onClicked: { root.zoomStart = 0; root.zoomEnd = root.totalMeters; }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label {
                    text: qsTr("CHANNELS")
                    color: "#8d9aaa"
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("%1/4").arg(root.visibleChannels.length)
                    color: "#687789"
                    font.pixelSize: 9
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                FeComboBox {
                    id: channelPicker
                    objectName: "comparisonAddChannelPicker"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 80
                    implicitHeight: 28
                    model: root.availableChannels.filter(channel => root.visibleChannels.indexOf(channel) < 0)
                    enabled: count > 0 && root.visibleChannels.length < 4
                }
                FeButton {
                    objectName: "comparisonAddChannel"
                    compact: true
                    text: qsTr("Add channel")
                    enabled: channelPicker.enabled
                    onClicked: root.toggleChannel(channelPicker.currentText)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                ComparisonOverlayMap {
                    id: overlayMap
                    Layout.preferredWidth: Math.max(160, root.width * 0.26)
                    Layout.fillHeight: true
                    hoverDistanceMeters: root.hoverDistanceMeters
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#070b10"
                    border.color: "#1c2631"
                    clip: true
                    SplitView {
                        id: chartSplit
                        anchors.fill: parent
                        anchors.margins: 6
                        orientation: Qt.Vertical
                        handle: Rectangle {
                            implicitHeight: 5
                            color: SplitHandle.pressed ? "#55e6a5" : SplitHandle.hovered ? "#334556" : "#14202a"
                        }
                        Repeater {
                            objectName: "comparisonChannelRows"
                            model: root.visibleChannels
                            delegate: Item {
                                id: rowItem
                                required property int index
                                required property var modelData
                                SplitView.fillWidth: true
                                SplitView.preferredHeight: Math.max(64, chartSplit.height / Math.max(1, root.visibleChannels.length))
                                SplitView.minimumHeight: 50
                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 2
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 4
                                        FeComboBox {
                                            objectName: "comparisonReplaceChannel-" + rowItem.modelData
                                            Layout.preferredWidth: 150
                                            implicitHeight: 22
                                            model: root.availableChannels.filter(
                                                channel => channel === rowItem.modelData || root.visibleChannels.indexOf(channel) < 0)
                                            currentIndex: model.indexOf(rowItem.modelData)
                                            onActivated: index => root.replaceChannel(rowItem.modelData, model[index])
                                        }
                                        ToolButton {
                                            objectName: "comparisonRemoveChannel-" + rowItem.modelData
                                            implicitWidth: 20
                                            implicitHeight: 22
                                            text: "×"
                                            Accessible.name: qsTr("Remove %1").arg(rowItem.modelData)
                                            onClicked: root.toggleChannel(rowItem.modelData)
                                        }
                                        Item { Layout.fillWidth: true }
                                    }
                                    ComparisonOverlayChart {
                                        objectName: "comparisonOverlayChart-" + rowItem.modelData
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        channel: rowItem.modelData
                                        zoomStart: root.zoomStart
                                        zoomEnd: root.zoomEnd
                                        totalMeters: root.totalMeters
                                        hoverDistanceMeters: root.hoverDistanceMeters
                                        onZoomRequested: (start, end) => { root.zoomStart = start; root.zoomEnd = end; }
                                        onHovered: meters => root.hoverDistanceMeters = meters
                                    }
                                }
                            }
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: root.visibleChannels.length === 0
                        text: qsTr("Add a telemetry channel to compare.")
                        color: "#657386"
                        font.pixelSize: 11
                    }
                }
            }
        }
    }
}
