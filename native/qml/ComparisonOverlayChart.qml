pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One channel row: both laps overlaid on a shared distance-into-lap x-axis,
// so a corner sits at roughly the same x position for A and B. Zoom/hover
// state is owned by the parent (not this row) so multiple channel rows and
// the track map all move together: this row only requests changes via
// signals and renders whatever zoomStart/zoomEnd/hoverDistanceMeters it is
// given. Meters here means shared track progress (KAN-31/32/33), not each
// lap's own distance-into-lap: a corner sits at the same x for both laps
// even on different racing lines. channel === "Δ time" is a synthetic
// pseudo-channel: the cumulative time gap between the laps
// (comparisonDeltaSeriesByProgress) rather than a per-lap telemetry channel,
// drawn as one line instead of an A/B overlay.
Item {
    id: root
    required property string channel
    readonly property bool isDeltaTime: root.channel === "Δ time"
    property real zoomStart: 0
    property real zoomEnd: 1
    property real totalMeters: 1
    property real hoverDistanceMeters: -1
    property color colorA: "#55e6a5"
    property color colorB: "#58bfff"
    property color colorDelta: "#ffcf5c"
    signal zoomRequested(real start, real end)
    signal hovered(real meters)

    // Drawn at the current zoom resolution. One point per pixel is already
    // more than a stroked line needs -- the earlier 1.5x oversampling only
    // added avoidable work to the hottest path (recomputed on every zoom/pan
    // step, times up to 4 visible rows).
    readonly property int pointBudget: Math.max(100, Math.min(1200, Math.round(plotArea.width)))
    readonly property var seriesA: root.isDeltaTime ? ({}) : (appController.comparisonSlots, appController.comparisonChannelSeriesByProgress(
        0, root.channel, root.zoomStart, root.zoomEnd, root.pointBudget))
    readonly property var seriesB: root.isDeltaTime ? ({}) : (appController.comparisonSlots, appController.comparisonChannelSeriesByProgress(
        1, root.channel, root.zoomStart, root.zoomEnd, root.pointBudget))
    readonly property var deltaSeries: !root.isDeltaTime ? ({}) : (appController.comparisonSlots, appController.comparisonDeltaSeriesByProgress(
        root.zoomStart, root.zoomEnd, root.pointBudget))

    // Fetched once over the whole lap (depends on comparisonSlots/totalMeters,
    // NOT zoomStart/zoomEnd), used only to fix the value axis. Rescaling the
    // axis to whatever sliver of data is visible while zooming/panning makes
    // an actually-tiny wobble look like a huge spike, and refitting it on
    // every wheel tick was also needless recompute on top of the zoom refetch.
    readonly property var fullRangeA: root.isDeltaTime ? ({}) : (appController.comparisonSlots, appController.comparisonChannelSeriesByProgress(
        0, root.channel, 0, root.totalMeters, 300))
    readonly property var fullRangeB: root.isDeltaTime ? ({}) : (appController.comparisonSlots, appController.comparisonChannelSeriesByProgress(
        1, root.channel, 0, root.totalMeters, 300))
    readonly property var fullRangeDelta: !root.isDeltaTime ? ({}) : (appController.comparisonSlots, appController.comparisonDeltaSeriesByProgress(
        0, root.totalMeters, 300))

    readonly property bool hasData: root.isDeltaTime
        ? (deltaSeries.segments || []).length > 0
        : ((seriesA.segments || []).length > 0 || (seriesB.segments || []).length > 0)
    readonly property string unit: root.isDeltaTime ? qsTr("s") : (seriesA.unit || seriesB.unit || "")

    readonly property bool hasRangeData: root.isDeltaTime
        ? !!fullRangeDelta.segments
        : !!(fullRangeA.segments || fullRangeB.segments)
    readonly property real rawLow: !hasRangeData ? 0 : (root.isDeltaTime
        ? Number(fullRangeDelta.minimum)
        : Math.min(fullRangeA.segments ? Number(fullRangeA.minimum) : Infinity,
                   fullRangeB.segments ? Number(fullRangeB.minimum) : Infinity))
    readonly property real rawHigh: !hasRangeData ? 1 : (root.isDeltaTime
        ? Number(fullRangeDelta.maximum)
        : Math.max(fullRangeA.segments ? Number(fullRangeA.maximum) : -Infinity,
                   fullRangeB.segments ? Number(fullRangeB.maximum) : -Infinity))
    readonly property real valuePadding: rawHigh === rawLow ? Math.max(0.5, Math.abs(rawLow) * 0.05) : (rawHigh - rawLow) * 0.08
    readonly property real valueLow: rawLow - valuePadding
    readonly property real valueHigh: rawHigh + valuePadding

    function graphY(value, low, high, height, brakingUp) {
        const fraction = (value - low) / Math.max(0.000001, high - low);
        return 3 + (brakingUp ? fraction : 1 - fraction) * Math.max(1, height - 6);
    }
    function valueAt(series, ratio) {
        const segments = series.segments || [];
        let best; let bestDistance = Infinity;
        for (const points of segments) {
            for (const point of points) {
                const distance = Math.abs(Number(point.x) - ratio);
                if (distance < bestDistance) { bestDistance = distance; best = point.y; }
            }
        }
        return best;
    }
    readonly property real hoverRatio: zoomEnd > zoomStart
        ? (hoverDistanceMeters - zoomStart) / (zoomEnd - zoomStart) : 0
    readonly property var hoverValueA: hoverDistanceMeters >= 0 && !root.isDeltaTime ? valueAt(seriesA, hoverRatio) : undefined
    readonly property var hoverValueB: hoverDistanceMeters >= 0 && !root.isDeltaTime ? valueAt(seriesB, hoverRatio) : undefined
    readonly property var hoverDelta: hoverValueA !== undefined && hoverValueB !== undefined
        ? hoverValueA - hoverValueB : undefined
    readonly property var hoverDeltaTime: hoverDistanceMeters >= 0 && root.isDeltaTime ? valueAt(deltaSeries, hoverRatio) : undefined

    Column {
        id: channelInfo
        objectName: "comparisonChannelInfo-" + root.channel
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(180, parent.width * 0.34, Math.max(140, parent.width * 0.24))
        spacing: 4
        Label {
            text: root.isDeltaTime ? qsTr("Δ time (A−B)") : root.channel
            color: "#dce4ee"
            font.pixelSize: 11
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            width: channelInfo.width
        }
        Row {
            visible: !root.isDeltaTime
            spacing: 6
            Rectangle { width: 9; height: 9; radius: 4.5; color: root.colorA; anchors.verticalCenter: parent.verticalCenter }
            Label {
                text: root.hoverValueA !== undefined ? Number(root.hoverValueA).toFixed(1) + " " + root.unit : "–"
                color: root.colorA
                font.family: "Menlo"
                font.pixelSize: 13
            }
        }
        Row {
            visible: !root.isDeltaTime
            spacing: 6
            Rectangle { width: 9; height: 9; radius: 4.5; color: root.colorB; anchors.verticalCenter: parent.verticalCenter }
            Label {
                text: root.hoverValueB !== undefined ? Number(root.hoverValueB).toFixed(1) + " " + root.unit : "–"
                color: root.colorB
                font.family: "Menlo"
                font.pixelSize: 13
            }
        }
        Label {
            visible: !root.isDeltaTime && root.hoverDelta !== undefined
            text: qsTr("Δ %1%2").arg(root.hoverDelta >= 0 ? "+" : "").arg((root.hoverDelta || 0).toFixed(1))
            color: "#f3f6fa"
            font.family: "Menlo"
            font.pixelSize: 13
            font.bold: true
        }
        Label {
            visible: root.isDeltaTime
            text: root.hoverDeltaTime !== undefined
                ? (root.hoverDeltaTime >= 0 ? "+" : "") + Number(root.hoverDeltaTime).toFixed(2) + " s"
                : "–"
            color: root.hoverDeltaTime === undefined ? "#dce4ee" : (root.hoverDeltaTime > 0 ? "#ff8a7a" : "#55e6a5")
            font.family: "Menlo"
            font.pixelSize: 16
            font.bold: true
        }
        Label {
            visible: root.isDeltaTime
            text: qsTr("+ = A behind")
            color: "#687789"
            font.pixelSize: 9
        }
    }

    Item {
        id: plotArea
        anchors.left: channelInfo.right
        anchors.leftMargin: 10
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        clip: true

        Canvas {
            id: chartCanvas
            anchors.fill: parent
            property var a: root.seriesA
            property var b: root.seriesB
            property var d: root.deltaSeries
            property real low: root.valueLow
            property real high: root.valueHigh
            onAChanged: requestPaint()
            onBChanged: requestPaint()
            onDChanged: requestPaint()
            onLowChanged: requestPaint()
            onHighChanged: requestPaint()
            onAvailableChanged: if (available) requestPaint()
            onVisibleChanged: if (visible) requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            function drawSeries(context, series, color, low, high) {
                const segments = series.segments || [];
                context.strokeStyle = color;
                context.lineWidth = 1.8;
                context.lineJoin = "round";
                for (let segmentIndex = 0; segmentIndex < segments.length; ++segmentIndex) {
                    const points = segments[segmentIndex];
                    if (points.length === 0) continue;
                    context.beginPath();
                    for (let pointIndex = 0; pointIndex < points.length; ++pointIndex) {
                        const x = Math.max(0, Math.min(width, Number(points[pointIndex].x) * width));
                        const y = root.graphY(Number(points[pointIndex].y), low, high, height, series.brakingUp === true);
                        if (pointIndex === 0) context.moveTo(x, y);
                        else context.lineTo(x, y);
                    }
                    context.stroke();
                    if (points.length === 1) {
                        context.fillStyle = color;
                        context.beginPath();
                        context.arc(Math.max(0, Math.min(width, Number(points[0].x) * width)),
                                    root.graphY(Number(points[0].y), low, high, height, series.brakingUp === true),
                                    2, 0, Math.PI * 2);
                        context.fill();
                    }
                }
            }
            onPaint: {
                const context = getContext("2d");
                context.clearRect(0, 0, width, height);
                context.strokeStyle = "#18232e";
                context.lineWidth = 1;
                for (let grid = 1; grid < 4; ++grid) {
                    const x = width * grid / 4;
                    context.beginPath();
                    context.moveTo(x, 0);
                    context.lineTo(x, height);
                    context.stroke();
                }
                if (!root.hasData) return;
                if (root.isDeltaTime) {
                    if (low < 0 && high > 0) {
                        const zeroY = root.graphY(0, low, high, height, false);
                        context.setLineDash([4, 4]);
                        context.strokeStyle = "#3a4a5c";
                        context.lineWidth = 1;
                        context.beginPath();
                        context.moveTo(0, zeroY);
                        context.lineTo(width, zeroY);
                        context.stroke();
                        context.setLineDash([]);
                    }
                    if (d.segments) drawSeries(context, d, root.colorDelta, low, high);
                } else {
                    if (a.segments) drawSeries(context, a, root.colorA, low, high);
                    if (b.segments) drawSeries(context, b, root.colorB, low, high);
                }
            }
        }

        Label {
            anchors.centerIn: parent
            visible: !root.hasData
            text: {
                const reason = root.seriesA.reason || root.seriesB.reason || "";
                switch (reason) {
                case "invalidRange": return qsTr("Selected range is invalid");
                case "channelMissing": return qsTr("Channel is not available in both laps");
                default: return qsTr("No data in selected range");
                }
            }
            color: (root.seriesA.reason || root.seriesB.reason) ? "#ffb84d" : "#657386"
            font.pixelSize: 10
        }

        Rectangle {
            visible: root.hoverDistanceMeters >= root.zoomStart && root.hoverDistanceMeters <= root.zoomEnd && !pointer.dragging
            x: Math.max(0, Math.min(parent.width - width,
                (root.hoverDistanceMeters - root.zoomStart) / Math.max(0.001, root.zoomEnd - root.zoomStart) * parent.width))
            width: 1
            height: parent.height
            color: "#f3f6fa"
            opacity: 0.8
        }

        Rectangle {
            id: zoomSelection
            // Must reference plotArea.width, not the bare "width": this
            // Rectangle's own width is what is being computed below, and an
            // unqualified "width" here resolves to that same (self) property,
            // creating a binding loop that gets stuck at 0 and never shows.
            visible: pointer.dragging
            readonly property real otherX: Math.max(0, Math.min(plotArea.width, pointer.mouseX))
            readonly property real selectedMeters: Math.abs(otherX - pointer.pressRatio * plotArea.width)
                / Math.max(1, plotArea.width) * (root.zoomEnd - root.zoomStart)
            x: Math.min(pointer.pressRatio * plotArea.width, otherX)
            width: Math.abs(otherX - pointer.pressRatio * plotArea.width)
            height: parent.height
            color: "#55e6a52a"
            border.color: "#55e6a5"
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 4
                text: qsTr("%1 m").arg(zoomSelection.selectedMeters.toFixed(0))
                color: "#0c150f"
                font.pixelSize: 10
                font.bold: true
                padding: 2
                background: Rectangle { color: "#55e6a5"; radius: 3 }
            }
        }

        MouseArea {
            id: pointer
            objectName: "comparisonChartPointer-" + root.channel
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.CrossCursor
            property real pressRatio: 0
            property bool dragging: false
            function distanceAt(fraction) {
                return root.zoomStart + Math.max(0, Math.min(1, fraction)) * (root.zoomEnd - root.zoomStart);
            }
            onPressed: mouse => {
                pointer.pressRatio = Math.max(0, Math.min(1, mouse.x / width));
                pointer.dragging = false;
            }
            onPositionChanged: mouse => {
                root.hovered(pointer.distanceAt(mouse.x / width));
                if (pressed && !pointer.dragging && Math.abs(mouse.x - pointer.pressRatio * width) > 4)
                    pointer.dragging = true;
            }
            onReleased: mouse => {
                if (pointer.dragging) {
                    const releaseRatio = Math.max(0, Math.min(1, mouse.x / width));
                    const span = root.zoomEnd - root.zoomStart;
                    const a = root.zoomStart + Math.min(pointer.pressRatio, releaseRatio) * span;
                    const b = root.zoomStart + Math.max(pointer.pressRatio, releaseRatio) * span;
                    if (b - a > Math.max(2, root.totalMeters * 0.005)) root.zoomRequested(a, b);
                }
                pointer.dragging = false;
            }
            onExited: root.hovered(-1)
            onDoubleClicked: root.zoomRequested(0, root.totalMeters)

            WheelHandler {
                id: wheel
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                // Wheel/trackpad events can fire far faster than the chart can
                // usefully redraw (each tick refetches every visible row's
                // series). Accumulate into pending values and only actually
                // apply -- and trigger the expensive refetch -- once per
                // throttle tick, using the still-pending target as the base
                // for the next tick so a fast scroll still feels continuous.
                property real pendingStart: -1
                property real pendingEnd: -1
                onWheel: event => {
                    const width = plotArea.width;
                    const centerRatio = Math.max(0, Math.min(1, event.x / width));
                    const baseStart = wheel.pendingStart >= 0 ? wheel.pendingStart : root.zoomStart;
                    const baseEnd = wheel.pendingEnd >= 0 ? wheel.pendingEnd : root.zoomEnd;
                    const span = baseEnd - baseStart;
                    const centerMeters = baseStart + centerRatio * span;
                    let newSpan = event.angleDelta.y !== 0
                        ? Math.max(2, Math.min(root.totalMeters, span * Math.pow(0.85, event.angleDelta.y / 120)))
                        : span;
                    let start = centerMeters - centerRatio * newSpan;
                    let end = start + newSpan;
                    if (event.angleDelta.x !== 0) {
                        const shift = -event.angleDelta.x / 120 * newSpan * 0.15;
                        start += shift; end += shift;
                    }
                    if (start < 0) { end -= start; start = 0; }
                    if (end > root.totalMeters) { start -= (end - root.totalMeters); end = root.totalMeters; }
                    wheel.pendingStart = Math.max(0, start);
                    wheel.pendingEnd = Math.min(root.totalMeters, end);
                    zoomThrottle.restart();
                }
            }
            Timer {
                id: zoomThrottle
                interval: 50
                repeat: false
                onTriggered: {
                    if (wheel.pendingStart >= 0) {
                        root.zoomRequested(wheel.pendingStart, wheel.pendingEnd);
                        wheel.pendingStart = -1;
                        wheel.pendingEnd = -1;
                    }
                }
            }
        }
    }
}
