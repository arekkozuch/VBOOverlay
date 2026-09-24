pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// One track map showing both A/B lap traces to the same scale (a shared
// bounding-box normalization, not each lap filling the frame on its own).
// hoverDistanceMeters (-1 when idle; a shared track-progress value, not each
// lap's own distance-into-lap) drives a position marker per lap at the
// corresponding point on the shared progress axis (KAN-37/38).
Rectangle {
    id: root
    property real hoverDistanceMeters: -1
    readonly property var trackA: appController.comparisonSlots.length
        ? (appController.comparisonOverlayTrack(0) || []) : []
    readonly property var trackB: appController.comparisonSlots.length
        ? (appController.comparisonOverlayTrack(1) || []) : []
    color: "#070b10"
    border.color: "#1c2631"
    radius: 8

    Label {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 10
        text: qsTr("TRACK POSITION")
        color: "#687789"
        font.pixelSize: 9
        font.letterSpacing: 1
    }

    Item {
        id: mapArea
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 10
        width: Math.max(0, Math.min(parent.width - 48, parent.height - 64))
        height: width

        Canvas {
            id: trackCanvas
            anchors.fill: parent
            property var segmentsA: root.trackA
            property var segmentsB: root.trackB
            onSegmentsAChanged: requestPaint()
            onSegmentsBChanged: requestPaint()
            onAvailableChanged: if (available) requestPaint()
            onVisibleChanged: if (visible) requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            function drawTrace(context, segments, color) {
                context.strokeStyle = color;
                context.lineWidth = 2.5;
                context.lineCap = "round";
                context.lineJoin = "round";
                for (const points of segments) {
                    if (points.length < 2) continue;
                    context.beginPath();
                    context.moveTo(Number(points[0].x) * width, Number(points[0].y) * height);
                    for (let index = 1; index < points.length; ++index)
                        context.lineTo(Number(points[index].x) * width, Number(points[index].y) * height);
                    context.stroke();
                }
            }
            onPaint: {
                const context = getContext("2d");
                context.reset();
                drawTrace(context, segmentsA, "#55e6a5");
                drawTrace(context, segmentsB, "#58bfff");
            }
        }
        Label {
            anchors.centerIn: parent
            visible: root.trackA.length === 0 && root.trackB.length === 0
            text: qsTr("No GPS data in this section")
            color: "#657386"
        }
        Repeater {
            model: 2
            delegate: Rectangle {
                id: marker
                required property int index
                readonly property var point: root.hoverDistanceMeters >= 0
                    ? appController.comparisonPositionAtProgress(index, root.hoverDistanceMeters) : ({})
                visible: point.x !== undefined
                width: 16
                height: 16
                radius: 8
                color: index === 0 ? "#55e6a5" : "#58bfff"
                border.color: "#0c150f"
                border.width: 2
                x: Number(point.x || 0) * mapArea.width - width / 2
                y: Number(point.y || 0) * mapArea.height - height / 2
            }
        }
    }
}
