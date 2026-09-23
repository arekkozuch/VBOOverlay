pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One channel, both laps overlaid on a shared distance-into-lap x-axis, so a
// corner sits at roughly the same x position for A and B. Drag to zoom into
// a section (e.g. one corner's trail-braking); hover reports the distance
// under the cursor so a track map can show both laps' positions there.
Rectangle {
    id: root
    required property string channel
    property real hoverDistanceMeters: -1
    color: "#070b10"
    border.color: "#1c2631"
    radius: 8
    clip: true

    // comparisonLapDistanceTotal/comparisonLapSeriesByDistance are Q_INVOKABLEs,
    // not properties: QML's automatic dependency tracking only follows real
    // property reads, so force one on comparisonSlots or these never
    // re-evaluate once a lap finishes loading.
    readonly property real totalMetersA: (appController.comparisonSlots, appController.comparisonLapDistanceTotal(0))
    readonly property real totalMetersB: (appController.comparisonSlots, appController.comparisonLapDistanceTotal(1))
    readonly property real totalMeters: Math.max(1, totalMetersA, totalMetersB)
    property real zoomStart: 0
    property real zoomEnd: totalMeters
    readonly property bool zoomed: zoomStart > 1e-3 || zoomEnd < totalMeters - 1e-3
    function resetZoom() { zoomStart = 0; zoomEnd = root.totalMeters; }
    onTotalMetersChanged: resetZoom()

    readonly property var seriesA: (appController.comparisonSlots, appController.comparisonLapSeriesByDistance(
        0, root.channel, root.zoomStart, root.zoomEnd, Math.max(100, Math.round(width * 1.5))))
    readonly property var seriesB: (appController.comparisonSlots, appController.comparisonLapSeriesByDistance(
        1, root.channel, root.zoomStart, root.zoomEnd, Math.max(100, Math.round(width * 1.5))))
    readonly property bool hasData: (seriesA.segments || []).length > 0 || (seriesB.segments || []).length > 0
    readonly property string unit: seriesA.unit || seriesB.unit || ""

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
    readonly property var hoverValueA: hoverDistanceMeters >= 0 ? valueAt(seriesA, hoverRatio) : undefined
    readonly property var hoverValueB: hoverDistanceMeters >= 0 ? valueAt(seriesB, hoverRatio) : undefined

    Canvas {
        id: chartCanvas
        anchors.fill: parent
        property var a: root.seriesA
        property var b: root.seriesB
        onAChanged: requestPaint()
        onBChanged: requestPaint()
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
            const rawLow = Math.min(
                a.segments ? Number(a.minimum) : Infinity, b.segments ? Number(b.minimum) : Infinity);
            const rawHigh = Math.max(
                a.segments ? Number(a.maximum) : -Infinity, b.segments ? Number(b.maximum) : -Infinity);
            const padding = rawHigh === rawLow ? Math.max(0.5, Math.abs(rawLow) * 0.05) : 0;
            const low = rawLow - padding;
            const high = rawHigh + padding;
            if (a.segments) drawSeries(context, a, "#55e6a5", low, high);
            if (b.segments) drawSeries(context, b, "#58bfff", low, high);
        }
    }

    Row {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 6
        spacing: 12
        visible: root.hoverDistanceMeters >= 0 && (root.hoverValueA !== undefined || root.hoverValueB !== undefined)
        Label {
            visible: root.hoverValueA !== undefined
            text: qsTr("A %1 %2").arg(Number(root.hoverValueA || 0).toFixed(1)).arg(root.unit)
            color: "#55e6a5"
            font.family: "Menlo"
            font.pixelSize: 10
        }
        Label {
            visible: root.hoverValueB !== undefined
            text: qsTr("B %1 %2").arg(Number(root.hoverValueB || 0).toFixed(1)).arg(root.unit)
            color: "#58bfff"
            font.family: "Menlo"
            font.pixelSize: 10
        }
        Label {
            visible: root.hoverValueA !== undefined && root.hoverValueB !== undefined
            text: qsTr("Δ %1").arg((root.hoverValueA - root.hoverValueB).toFixed(1))
            color: "#dce4ee"
            font.family: "Menlo"
            font.pixelSize: 10
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
        visible: pointer.dragging
        readonly property real otherX: Math.max(0, Math.min(width, pointer.mouseX))
        readonly property real selectedMeters: Math.abs(otherX - pointer.pressRatio * width)
            / Math.max(1, width) * (root.zoomEnd - root.zoomStart)
        x: Math.min(pointer.pressRatio * width, otherX)
        width: Math.abs(otherX - pointer.pressRatio * width)
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
        objectName: "comparisonChartPointer"
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
            root.hoverDistanceMeters = pointer.distanceAt(mouse.x / width);
            if (pressed && !pointer.dragging && Math.abs(mouse.x - pointer.pressRatio * width) > 4)
                pointer.dragging = true;
        }
        onReleased: mouse => {
            if (pointer.dragging) {
                const releaseRatio = Math.max(0, Math.min(1, mouse.x / width));
                const span = root.zoomEnd - root.zoomStart;
                const a = root.zoomStart + Math.min(pointer.pressRatio, releaseRatio) * span;
                const b = root.zoomStart + Math.max(pointer.pressRatio, releaseRatio) * span;
                if (b - a > Math.max(2, root.totalMeters * 0.005)) {
                    root.zoomStart = a; root.zoomEnd = b;
                }
            }
            pointer.dragging = false;
        }
        onExited: root.hoverDistanceMeters = -1
        onDoubleClicked: root.resetZoom()
    }

    FeButton {
        objectName: "comparisonResetZoom"
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 6
        visible: root.zoomed
        compact: true
        text: qsTr("Reset zoom (%1 m)").arg((root.zoomEnd - root.zoomStart).toFixed(0))
        onClicked: root.resetZoom()
    }
}
