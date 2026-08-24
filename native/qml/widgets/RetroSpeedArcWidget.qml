import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property var frame: parent.frame
    anchors.fill: parent
    property var rawValue: frame.adjusted(frame.raw("source", "speed"))
    property bool hasValue: rawValue !== undefined && rawValue !== null && Number.isFinite(Number(rawValue))
    property real value: hasValue ? Number(rawValue) : 0
    Canvas {
        id: retroSpeedCanvas
        anchors.fill: parent
        property real value: parent.value
        onValueChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const settings = frame.widgetSettings;
            const minimum = Number(settings.minValue ?? 0);
            const maximum = Math.max(minimum + 1, Number(settings.maxValue ?? 360));
            const progress = Math.max(0, Math.min(1, (value - minimum) / (maximum - minimum)));
            const segments = Math.max(5, Math.min(40, Number(settings.segments ?? 19)));
            const cx = width * 0.28;
            const cy = height * 0.88;
            const radius = Math.min(width * 0.50, height * 0.78);
            if (!Number.isFinite(radius) || radius <= 0)
                return;
            const start = Math.PI * 0.92;
            const end = Math.PI * 1.82;
            ctx.lineWidth = Math.max(5 * frame.sceneScale, radius * 0.12);
            for (let segment = 0; segment < segments; ++segment) {
                const fraction = segment / (segments - 1);
                const a0 = start + (end - start) * segment / segments;
                const a1 = start + (end - start) * (segment + 0.72) / segments;
                if (fraction <= progress) {
                    ctx.strokeStyle = fraction < 0.58 ? (settings.lowColor || "#00bd31") : (fraction < 0.82 ? (settings.midColor || "#f2e920") : (settings.highColor || "#ff9124"));
                    ctx.globalAlpha = 1;
                } else {
                    ctx.strokeStyle = settings.emptyColor || "#d8d8d8";
                    ctx.globalAlpha = 0.25;
                }
                ctx.beginPath();
                ctx.arc(cx, cy, radius, a0, a1);
                ctx.stroke();
            }
            ctx.globalAlpha = 1;
        }
    }
    Label {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        text: (parent.hasValue ? Math.round(parent.value).toString() : "—") + " " + (frame.widgetSettings.unit || "Km/h")
        color: frame.primary
        font.family: frame.family
        font.weight: Font.Bold
        font.pixelSize: frame.configuredFontSize() > 0
            ? frame.configuredFontSize() * frame.sceneScale
            : Math.max(9 * frame.sceneScale, Math.min(parent.height * 0.12, parent.width * 0.08))
    }
    Connections {
        target: frame.widgetModel
        function onRevisionChanged() {
            retroSpeedCanvas.requestPaint();
        }
    }
}
