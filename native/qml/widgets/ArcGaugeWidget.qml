import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property var frame: parent.frame
    anchors.fill: parent
    property var gaugeRaw: frame.raw("source", "speed")
    property real gaugeValue: Number(frame.adjusted(gaugeRaw) || 0)
    property real minimum: Number(frame.widgetSettings.minValue ?? 0)
    property real maximum: Math.max(minimum + 0.001, Number(frame.widgetSettings.maxValue ?? 300))
    property real progress: Math.max(0, Math.min(1, (gaugeValue - minimum) / (maximum - minimum)))
    Canvas {
        id: arcCanvas
        anchors.fill: parent
        property real progress: parent.progress
        property real startAngle: Number(frame.widgetSettings.startAngle ?? 155)
        property real endAngle: Number(frame.widgetSettings.endAngle ?? 385)
        property real arcWidth: Number(frame.widgetSettings.arcWidth ?? 12) * frame.sceneScale
        onProgressChanged: requestPaint()
        onStartAngleChanged: requestPaint()
        onEndAngleChanged: requestPaint()
        onArcWidthChanged: requestPaint()
        onPaint: {
            const context = getContext("2d");
            context.reset();
            const start = startAngle * Math.PI / 180;
            const end = endAngle * Math.PI / 180;
            const radius = Math.max(4 * frame.sceneScale, Math.min(width, height) * 0.38);
            const centerX = width / 2;
            const centerY = height * 0.52;
            context.lineCap = "round";
            context.lineWidth = arcWidth;
            context.strokeStyle = frame.widgetSettings.trackColor || "#263442";
            context.beginPath();
            context.arc(centerX, centerY, radius, start, end, false);
            context.stroke();
            context.strokeStyle = frame.accent;
            context.beginPath();
            context.arc(centerX, centerY, radius, start, start + (end - start) * progress, false);
            context.stroke();
        }
    }
    Column {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: parent.height * 0.08
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: frame.widgetSettings.showValue ?? true
            text: frame.numberText(parent.parent.gaugeRaw, 1)
            color: frame.primary
            font.family: frame.family
            font.weight: frame.weight
            font.pixelSize: Math.min(34 * frame.sceneScale, frame.height * 0.24) * frame.valueScale
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: (frame.widgetSettings.label || "GAUGE") + ((frame.widgetSettings.showUnit ?? true) ? "  " + (frame.widgetSettings.unit || "") : "")
            color: frame.secondary
            font.family: frame.family
            font.pixelSize: 9 * frame.labelScale * frame.sceneScale
            font.letterSpacing: frame.sceneScale
        }
    }
    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: frame.widgetSettings.showMinMax ?? true
        Label {
            text: parent.parent.minimum.toFixed(0)
            color: frame.secondary
            font.pixelSize: 8 * frame.sceneScale
        }
        Item {
            Layout.fillWidth: true
        }
        Label {
            text: parent.parent.maximum.toFixed(0)
            color: frame.secondary
            font.pixelSize: 8 * frame.sceneScale
        }
    }
    Connections {
        target: frame.widgetModel
        function onRevisionChanged() {
            arcCanvas.requestPaint();
        }
    }
}
