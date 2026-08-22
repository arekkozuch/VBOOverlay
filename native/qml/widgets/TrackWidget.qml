import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property var frame: parent.frame
    anchors.fill: parent
    property var currentPoint: {
        frame.renderContext.time;
        return frame.renderContext.currentTrackPoint;
    }
    property real trackPad: Number(frame.widgetSettings.trackPadding ?? 10)
    Canvas {
        id: trackCanvas
        anchors.fill: parent
        onPaint: {
            const context = getContext("2d");
            context.reset();
            const points = frame.renderContext.trackPoints;
            if (points.length < 2)
                return;
            const pad = parent.trackPad;
            const drawX = value => pad + (frame.widgetSettings.mirrorX ? 1 - value : value) * Math.max(1, width - 2 * pad);
            const drawY = value => pad + (frame.widgetSettings.mirrorY ? 1 - value : value) * Math.max(1, height - 2 * pad);
            context.strokeStyle = frame.widgetSettings.lineColor || frame.accent;
            context.lineWidth = Number(frame.widgetSettings.lineWidth ?? 3);
            context.lineCap = "round";
            context.lineJoin = "round";
            context.beginPath();
            context.moveTo(drawX(points[0].x), drawY(points[0].y));
            for (let pointIndex = 1; pointIndex < points.length; ++pointIndex)
                context.lineTo(drawX(points[pointIndex].x), drawY(points[pointIndex].y));
            context.stroke();
        }
        Connections {
            target: frame.renderContext
            function onSourceChanged() {
                trackCanvas.requestPaint();
            }
            function onTimeChanged() {
                trackCanvas.requestPaint();
            }
        }
    }
    Rectangle {
        visible: parent.currentPoint.x !== undefined
        width: Number(frame.widgetSettings.markerSize ?? 10)
        height: width
        radius: width / 2
        color: frame.widgetSettings.markerColor || "#ffffff"
        x: parent.trackPad + (frame.widgetSettings.mirrorX ? 1 - Number(parent.currentPoint.x || 0) : Number(parent.currentPoint.x || 0)) * Math.max(1, parent.width - 2 * parent.trackPad) - width / 2
        y: parent.trackPad + (frame.widgetSettings.mirrorY ? 1 - Number(parent.currentPoint.y || 0) : Number(parent.currentPoint.y || 0)) * Math.max(1, parent.height - 2 * parent.trackPad) - height / 2
    }
    Connections {
        target: frame.widgetModel
        function onRevisionChanged() {
            trackCanvas.requestPaint();
        }
    }
}
