pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
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
        anchors.fill: parent
        anchors.margins: 24
        anchors.topMargin: 32
        property var currentPoint: {
            appController.playbackTime;
            return appController.currentTrackPoint;
        }

        Canvas {
            id: trackCanvas
            anchors.fill: parent
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const context = getContext("2d");
                context.reset();
                const points = appController.trackPoints;
                if (points.length < 2)
                    return;
                context.strokeStyle = "#536779";
                context.lineWidth = 2.5;
                context.lineCap = "round";
                context.lineJoin = "round";
                context.beginPath();
                context.moveTo(Number(points[0].x) * width, Number(points[0].y) * height);
                for (let index = 1; index < points.length; ++index)
                    context.lineTo(Number(points[index].x) * width, Number(points[index].y) * height);
                context.stroke();
            }
            Connections {
                target: appController
                function onTelemetryChanged() {
                    trackCanvas.requestPaint();
                }
            }
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
