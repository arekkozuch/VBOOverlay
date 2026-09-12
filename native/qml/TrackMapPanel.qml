pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property bool lapDetail: false
    readonly property var pathSegments: lapDetail ? appController.outingLapTrack : [appController.trackPoints]
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
        property var currentPoint: {
            if (root.lapDetail) return appController.outingLapTrackPoint;
            appController.playbackTime;
            return appController.currentTrackPoint;
        }

        Canvas {
            id: trackCanvas
            anchors.fill: parent
            property var segments: root.pathSegments
            onSegmentsChanged: requestPaint()
            onAvailableChanged: if (available) requestPaint()
            onVisibleChanged: if (visible) requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const context = getContext("2d");
                context.reset();
                context.strokeStyle = "#536779";
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
            Connections {
                target: appController
                function onTelemetryChanged() {
                    trackCanvas.requestPaint();
                }
            }
        }
        Label {
            anchors.centerIn: parent
            visible: root.lapDetail && root.pathSegments.length === 0
            text: qsTr("No GPS data in this section")
            color: "#657386"
        }
        Rectangle {
            visible: mapArea.currentPoint.x !== undefined
            width: 10
            height: 10
            radius: 5
            color: "#55e6a5"
            border.color: "#d9fff0"
            x: Number(mapArea.currentPoint.x || 0) * mapArea.width - width / 2
            y: Number(mapArea.currentPoint.y || 0) * mapArea.height - height / 2
        }
    }
}
