pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// KAN-55: Corner Analyzer -- approved-segment list with A/B/delta metrics
// (sector time; entry/apex/minimum/exit speed, braking point and throttle
// pickup for corners) for the current comparison pair. Only ever populated
// when both compared laps' approved segmentation matches exactly (same
// revision) -- comparisonApprovedSegments() returns nothing otherwise, and
// this panel simply stays empty rather than guessing a correspondence
// between two independently-approved segment sets. Selecting a metric row
// sets the shared progress cursor/range (rangeRequested/hovered), the same
// mechanism ComparisonOverlayChart/ComparisonOverlayMap already drive from
// hoverDistanceMeters/zoomStart/zoomEnd.
Rectangle {
    id: root
    color: "#070b10"
    border.color: "#1c2631"
    radius: 8
    signal rangeRequested(real startMeters, real endMeters)
    signal hovered(real meters)

    // appController.comparisonSlots is a real Q_PROPERTY (NOTIFY
    // comparisonSlotsChanged); forcing a read on it here is the same
    // established pattern comparisonChannelSeriesByProgress already uses in
    // ComparisonOverlayChart.qml so this Q_INVOKABLE re-runs whenever the
    // pair actually changes, not a new/different mechanism.
    readonly property var segments: (appController.comparisonSlots, appController.comparisonApprovedSegments())
    property string selectedSegmentId: ""
    readonly property var metrics: root.selectedSegmentId.length > 0
        ? (appController.comparisonSlots, appController.comparisonSegmentMetrics(root.selectedSegmentId)) : ({})
    onSegmentsChanged: {
        if (root.segments.length === 0) { root.selectedSegmentId = ""; return; }
        if (!root.segments.some(segment => segment.id === root.selectedSegmentId))
            root.selectedSegmentId = root.segments[0].id;
        // Deferred: the same pair change also resets the shared zoom window
        // (ComparisonDetailPanel.onTotalMetersChanged); apply after it.
        Qt.callLater(root.applyRequestedSegment);
    }
    // KAN-57: a segment requested from elsewhere (the theoretical-best
    // dialog). Applied once the requested pair's segments are available, then
    // cleared so later list changes do not keep jumping back to it.
    readonly property string requestedSegmentId: appController.comparisonFocusSegmentId
    onRequestedSegmentIdChanged: root.applyRequestedSegment()
    function applyRequestedSegment() {
        if (root.requestedSegmentId.length === 0) return;
        const segment = root.segments.find(candidate => candidate.id === root.requestedSegmentId);
        if (!segment) return;
        root.selectedSegmentId = segment.id;
        if (segment.endMeters > segment.startMeters) root.selectMetric(segment.startMeters, segment.endMeters);
        Qt.callLater(() => appController.clearComparisonFocusSegment());
    }

    function selectMetric(startMeters, endMeters) {
        root.rangeRequested(startMeters, endMeters);
        root.hovered((startMeters + endMeters) / 2);
    }
    function formatValue(entry, digits, suffix) {
        if (!entry || entry.value === undefined) return "—";
        return Number(entry.value).toFixed(digits) + (suffix || "");
    }
    function formatDelta(entry, digits, suffix) {
        if (!entry || entry.value === undefined) return "—";
        const value = Number(entry.value);
        return (value >= 0 ? "+" : "") + value.toFixed(digits) + (suffix || "");
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8
        Label {
            text: qsTr("CORNER ANALYZER")
            color: "#8d9aaa"
            font.pixelSize: 9
            font.weight: Font.DemiBold
            font.letterSpacing: 1
        }
        Label {
            objectName: "cornerAnalyzerSegmentationNote"
            Layout.fillWidth: true
            visible: text.length > 0
            text: (appController.comparisonSlots, appController.comparisonSegmentationNote())
            color: "#d6a457"
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
            font.pixelSize: 11
        }
        Label {
            objectName: "cornerAnalyzerEmptyMessage"
            Layout.fillWidth: true
            visible: root.segments.length === 0
            text: qsTr("No matching approved segments for these two laps. Approve the same track segmentation on both to use the Corner Analyzer.")
            color: "#657386"
            wrapMode: Text.WordWrap
            font.pixelSize: 11
        }
        RowLayout {
            visible: root.segments.length > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10
            ListView {
                objectName: "cornerAnalyzerSegmentList"
                Layout.preferredWidth: 150
                Layout.fillHeight: true
                clip: true
                model: root.segments
                delegate: ItemDelegate {
                    id: segmentDelegate
                    required property var modelData
                    objectName: "cornerAnalyzerSegment-" + modelData.id
                    width: ListView.view ? ListView.view.width : implicitWidth
                    highlighted: modelData.id === root.selectedSegmentId
                    text: modelData.name + " (" + modelData.type + ")"
                    onClicked: {
                        root.selectedSegmentId = modelData.id;
                        root.selectMetric(modelData.startMeters, modelData.endMeters);
                    }
                }
            }
            ColumnLayout {
                objectName: "cornerAnalyzerMetrics"
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 3
                RowLayout {
                    Layout.fillWidth: true
                    visible: Object.keys(root.metrics).length > 0
                    Label { text: qsTr("A"); color: "#55e6a5"; Layout.preferredWidth: 90; font.weight: Font.DemiBold }
                    Label { text: qsTr("B"); color: "#d95926"; Layout.preferredWidth: 90; font.weight: Font.DemiBold }
                    Label { text: qsTr("Δ (A−B)"); color: "#f3f6fa"; font.weight: Font.DemiBold }
                }
                RowLayout {
                    objectName: "cornerAnalyzerSectorTimeRow"
                    Layout.fillWidth: true
                    visible: root.metrics.sectorTime !== undefined
                    Label { text: qsTr("Sector time"); color: "#91a0b2"; Layout.preferredWidth: 110 }
                    Label { objectName: "cornerAnalyzerSectorTimeA"; text: root.formatValue(root.metrics.sectorTime && root.metrics.sectorTime.a, 3, " s"); color: "#55e6a5"; Layout.preferredWidth: 90 }
                    Label { objectName: "cornerAnalyzerSectorTimeB"; text: root.formatValue(root.metrics.sectorTime && root.metrics.sectorTime.b, 3, " s"); color: "#d95926"; Layout.preferredWidth: 90 }
                    Label { objectName: "cornerAnalyzerSectorTimeDelta"; text: root.formatDelta(root.metrics.sectorTime && root.metrics.sectorTime.delta, 3, " s"); color: "#f3f6fa"; font.bold: true }
                    Item { Layout.fillWidth: true }
                    ToolButton {
                        objectName: "cornerAnalyzerSectorTimeSelect"
                        text: qsTr("⌖")
                        visible: root.segments.length > 0
                        Accessible.name: qsTr("Jump to this segment")
                        onClicked: {
                            const segment = root.segments.find(s => s.id === root.selectedSegmentId);
                            if (segment) root.selectMetric(segment.startMeters, segment.endMeters);
                        }
                    }
                }
                Repeater {
                    objectName: "cornerAnalyzerCornerSpeedRows"
                    model: root.metrics.corner !== undefined ? ["entry", "apex", "minimum", "exit"] : []
                    delegate: RowLayout {
                        id: speedRow
                        required property string modelData
                        objectName: "cornerAnalyzerSpeedRow-" + modelData
                        Layout.fillWidth: true
                        readonly property var phase: root.metrics.corner ? root.metrics.corner[speedRow.modelData] : undefined
                        Label {
                            text: speedRow.modelData.charAt(0).toUpperCase() + speedRow.modelData.slice(1) + qsTr(" speed")
                            color: "#91a0b2"; Layout.preferredWidth: 110
                        }
                        Label { text: root.formatValue(speedRow.phase && speedRow.phase.a, 1, " " + (root.metrics.corner ? root.metrics.corner.unit : "")); color: "#55e6a5"; Layout.preferredWidth: 90 }
                        Label { text: root.formatValue(speedRow.phase && speedRow.phase.b, 1, " " + (root.metrics.corner ? root.metrics.corner.unit : "")); color: "#d95926"; Layout.preferredWidth: 90 }
                        Label { text: root.formatDelta(speedRow.phase && speedRow.phase.delta, 1); color: "#f3f6fa"; font.bold: true }
                    }
                }
                RowLayout {
                    id: brakingRow
                    objectName: "cornerAnalyzerBrakingRow"
                    Layout.fillWidth: true
                    visible: root.metrics.braking !== undefined
                    readonly property var brakingPoint: root.metrics.braking ? root.metrics.braking.point : undefined
                    Label { text: qsTr("Braking point"); color: "#91a0b2"; Layout.preferredWidth: 110 }
                    Label { text: root.formatValue(brakingRow.brakingPoint && brakingRow.brakingPoint.a, 1, " m"); color: "#55e6a5"; Layout.preferredWidth: 90 }
                    Label { text: root.formatValue(brakingRow.brakingPoint && brakingRow.brakingPoint.b, 1, " m"); color: "#d95926"; Layout.preferredWidth: 90 }
                    Label { text: root.formatDelta(brakingRow.brakingPoint && brakingRow.brakingPoint.delta, 1, " m"); color: "#f3f6fa"; font.bold: true }
                }
                RowLayout {
                    id: exitRow
                    objectName: "cornerAnalyzerExitRow"
                    Layout.fillWidth: true
                    visible: root.metrics.exitEffects !== undefined
                    readonly property var pickupMetric: root.metrics.exitEffects ? root.metrics.exitEffects.pickup : undefined
                    Label { text: qsTr("Throttle pickup"); color: "#91a0b2"; Layout.preferredWidth: 110 }
                    Label { text: root.formatValue(exitRow.pickupMetric && exitRow.pickupMetric.a, 1, " m"); color: "#55e6a5"; Layout.preferredWidth: 90 }
                    Label { text: root.formatValue(exitRow.pickupMetric && exitRow.pickupMetric.b, 1, " m"); color: "#d95926"; Layout.preferredWidth: 90 }
                    Label { text: root.formatDelta(exitRow.pickupMetric && exitRow.pickupMetric.delta, 1, " m"); color: "#f3f6fa"; font.bold: true }
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
