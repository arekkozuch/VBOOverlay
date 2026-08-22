import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property var frame: parent.frame
    anchors.fill: parent
    property var gaugeRaw: frame.raw("source", "rpm")
    property real gaugeValue: Number(frame.adjusted(gaugeRaw) || 0)
    property real minimum: Number(frame.widgetSettings.minValue ?? 0)
    property real maximum: Math.max(minimum + 0.001, Number(frame.widgetSettings.maxValue ?? 8000))
    Canvas {
        id: dialCanvas
        anchors.fill: parent
        property real gaugeValue: parent.gaugeValue
        property real minimum: parent.minimum
        property real maximum: parent.maximum
        property real startAngle: Number(frame.widgetSettings.startAngle ?? 140)
        property real endAngle: Number(frame.widgetSettings.endAngle ?? 400)
        property int majorTicks: Math.max(2, Number(frame.widgetSettings.majorTicks ?? 8))
        property int minorTicks: Math.max(0, Number(frame.widgetSettings.minorTicks ?? 4))
        onGaugeValueChanged: requestPaint()
        onMinimumChanged: requestPaint()
        onMaximumChanged: requestPaint()
        onStartAngleChanged: requestPaint()
        onEndAngleChanged: requestPaint()
        onMajorTicksChanged: requestPaint()
        onMinorTicksChanged: requestPaint()
        onPaint: {
            const context = getContext("2d");
            context.reset();
            const cx = width / 2;
            const cy = height * 0.5;
            const radius = Math.max(6, Math.min(width, height) * 0.38);
            const start = startAngle * Math.PI / 180;
            const end = endAngle * Math.PI / 180;
            const totalTicks = (majorTicks - 1) * (minorTicks + 1);
            if (frame.widgetSettings.showTicks ?? true) {
                context.strokeStyle = frame.widgetSettings.tickColor || "#8290a0";
                context.lineCap = "round";
                for (let tick = 0; tick <= totalTicks; ++tick) {
                    const angle = start + (end - start) * tick / totalTicks;
                    const major = tick % (minorTicks + 1) === 0;
                    const outer = radius;
                    const inner = radius - (major ? 11 : 6);
                    context.lineWidth = major ? 2 : 1;
                    context.beginPath();
                    context.moveTo(cx + Math.cos(angle) * inner, cy + Math.sin(angle) * inner);
                    context.lineTo(cx + Math.cos(angle) * outer, cy + Math.sin(angle) * outer);
                    context.stroke();
                }
            }
            const progress = Math.max(0, Math.min(1, (gaugeValue - minimum) / (maximum - minimum)));
            const needleAngle = start + (end - start) * progress;
            context.strokeStyle = frame.widgetSettings.needleColor || "#ff5b63";
            context.lineWidth = 3;
            context.beginPath();
            context.moveTo(cx - Math.cos(needleAngle) * radius * 0.12, cy - Math.sin(needleAngle) * radius * 0.12);
            context.lineTo(cx + Math.cos(needleAngle) * radius * 0.72, cy + Math.sin(needleAngle) * radius * 0.72);
            context.stroke();
            context.fillStyle = frame.widgetSettings.needleColor || "#ff5b63";
            context.beginPath();
            context.arc(cx, cy, 5, 0, Math.PI * 2, false);
            context.fill();
        }
    }
    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: frame.widgetSettings.showValue ?? true
            text: frame.numberText(parent.parent.gaugeRaw, 1)
            color: frame.primary
            font.family: frame.family
            font.weight: frame.weight
            font.pixelSize: Math.min(26, frame.height * 0.18) * frame.valueScale
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: (frame.widgetSettings.label || "GAUGE") + ((frame.widgetSettings.showUnit ?? true) ? "  " + (frame.widgetSettings.unit || "") : "")
            color: frame.secondary
            font.family: frame.family
            font.pixelSize: 8 * frame.labelScale
            font.letterSpacing: 0.8
        }
    }
    Connections {
        target: frame.widgetModel
        function onRevisionChanged() {
            dialCanvas.requestPaint();
        }
    }
}
