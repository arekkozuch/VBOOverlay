pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One channel row: both laps overlaid on a shared distance-into-lap x-axis,
// so a corner sits at roughly the same x position for A and B. Zoom/hover
// state is owned by the parent (not this row) so multiple channel rows and
// the track map all move together: this row only requests changes via
// signals and renders whatever zoomStart/zoomEnd/hoverDistanceMeters it is
// given.
Item {
    id: root
    required property string channel
    property real zoomStart: 0
    property real zoomEnd: 1
    property real totalMeters: 1
    property real hoverDistanceMeters: -1
    property color colorA: "#55e6a5"
    property color colorB: "#58bfff"
    signal zoomRequested(real start, real end)
    signal hovered(real meters)

    readonly property var seriesA: (appController.comparisonSlots, appController.comparisonLapSeriesByDistance(
        0, root.channel, root.zoomStart, root.zoomEnd, Math.max(100, Math.round(plotArea.width * 1.5))))
    readonly property var seriesB: (appController.comparisonSlots, appController.comparisonLapSeriesByDistance(
        1, root.channel, root.zoomStart, root.zoomEnd, Math.max(100, Math.round(plotArea.width * 1.5))))
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
    readonly property var hoverDelta: hoverValueA !== undefined && hoverValueB !== undefined
        ? hoverValueA - hoverValueB : undefined

    Column {
        id: channelInfo
        objectName: "comparisonChannelInfo-" + root.channel
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(180, parent.width * 0.34, Math.max(140, parent.width * 0.24))
        spacing: 4
        Label {
            text: root.channel
            color: "#dce4ee"
            font.pixelSize: 11
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            width: channelInfo.width
        }
        Row {
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
            visible: root.hoverDelta !== undefined
            text: qsTr("Δ %1%2").arg(root.hoverDelta >= 0 ? "+" : "").arg((root.hoverDelta || 0).toFixed(1))
            color: "#f3f6fa"
            font.family: "Menlo"
            font.pixelSize: 13
            font.bold: true
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
                if (a.segments) drawSeries(context, a, root.colorA, low, high);
                if (b.segments) drawSeries(context, b, root.colorB, low, high);
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
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: event => {
                    const width = plotArea.width;
                    const centerRatio = Math.max(0, Math.min(1, event.x / width));
                    const span = root.zoomEnd - root.zoomStart;
                    const centerMeters = root.zoomStart + centerRatio * span;
                    // Vertical scroll zooms (centered under the cursor); horizontal
                    // scroll pans -- the two-finger trackpad gestures users expect,
                    // rather than only being able to re-drag a fresh selection box.
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
                    root.zoomRequested(Math.max(0, start), Math.min(root.totalMeters, end));
                }
            }
        }
    }
}
