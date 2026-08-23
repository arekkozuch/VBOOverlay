import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

Item {
    id: root
    property var frame: parent.frame
    anchors.fill: parent
    property var currentPoint: {
        frame.renderContext.time;
        return frame.renderContext.currentTrackPoint;
    }
    property real trackPad: Number(frame.widgetSettings.trackPadding ?? 10) * frame.sceneScale
    property color trackLineColor: frame.widgetSettings.lineColor || frame.accent
    property real trackLineWidth: Number(frame.widgetSettings.lineWidth ?? 3) * frame.sceneScale
    property bool trackMirrorX: frame.widgetSettings.mirrorX ?? false
    property bool trackMirrorY: frame.widgetSettings.mirrorY ?? false
    property var renderedTrackPoints: {
        frame.renderContext.trackRevision;
        const points = frame.renderContext.trackPoints;
        const result = new Array(points.length);
        const drawWidth = Math.max(1, width - 2 * trackPad);
        const drawHeight = Math.max(1, height - 2 * trackPad);
        for (let pointIndex = 0; pointIndex < points.length; ++pointIndex) {
            const point = points[pointIndex];
            const xValue = trackMirrorX ? 1 - point.x : point.x;
            const yValue = trackMirrorY ? 1 - point.y : point.y;
            result[pointIndex] = Qt.point(
                trackPad + xValue * drawWidth,
                trackPad + yValue * drawHeight);
        }
        return result;
    }
    Shape {
        anchors.fill: parent
        visible: root.renderedTrackPoints.length >= 2
        ShapePath {
            strokeColor: root.trackLineColor
            strokeWidth: root.trackLineWidth
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            fillColor: "transparent"
            PathPolyline {
                path: root.renderedTrackPoints
            }
        }
    }
    Rectangle {
        visible: parent.currentPoint.x !== undefined
        width: Number(frame.widgetSettings.markerSize ?? 10) * frame.sceneScale
        height: width
        radius: width / 2
        color: frame.widgetSettings.markerColor || "#ffffff"
        x: root.trackPad + (root.trackMirrorX ? 1 - Number(root.currentPoint.x || 0) : Number(root.currentPoint.x || 0)) * Math.max(1, root.width - 2 * root.trackPad) - width / 2
        y: root.trackPad + (root.trackMirrorY ? 1 - Number(root.currentPoint.y || 0) : Number(root.currentPoint.y || 0)) * Math.max(1, root.height - 2 * root.trackPad) - height / 2
    }
}
