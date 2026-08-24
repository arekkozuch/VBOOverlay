import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property var frame: parent.frame
    anchors.fill: parent
    property var rawValue: frame.adjusted(frame.raw("source", "rpm"))
    property bool hasValue: rawValue !== undefined && rawValue !== null && Number.isFinite(Number(rawValue))
    property real value: hasValue ? Number(rawValue) : Number(frame.widgetSettings.minValue ?? 0)
    Canvas {
        id: retroTachometerCanvas
        anchors.fill: parent
        property real value: parent.value
        property bool hasValue: parent.hasValue
        onValueChanged: requestPaint()
        onHasValueChanged: requestPaint()
        function roundedPath(ctx, x, y, rectangleWidth, rectangleHeight, radius) {
            const boundedRadius = Math.min(radius, rectangleWidth / 2, rectangleHeight / 2);
            ctx.beginPath();
            ctx.moveTo(x + boundedRadius, y);
            ctx.lineTo(x + rectangleWidth - boundedRadius, y);
            ctx.quadraticCurveTo(x + rectangleWidth, y, x + rectangleWidth, y + boundedRadius);
            ctx.lineTo(x + rectangleWidth, y + rectangleHeight - boundedRadius);
            ctx.quadraticCurveTo(x + rectangleWidth, y + rectangleHeight, x + rectangleWidth - boundedRadius, y + rectangleHeight);
            ctx.lineTo(x + boundedRadius, y + rectangleHeight);
            ctx.quadraticCurveTo(x, y + rectangleHeight, x, y + rectangleHeight - boundedRadius);
            ctx.lineTo(x, y + boundedRadius);
            ctx.quadraticCurveTo(x, y, x + boundedRadius, y);
            ctx.closePath();
        }
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const settings = frame.widgetSettings;
            const cx = width / 2;
            const cy = height * 0.52;
            const radius = Math.min(width, height) * 0.31;
            const minimum = Number(settings.minValue ?? 0);
            const maximum = Math.max(minimum + 1, Number(settings.maxValue ?? 8000));
            const steps = Math.max(4, Math.min(16, Math.round((maximum - minimum) / 1000)));
            const progress = Math.max(0, Math.min(1, (value - minimum) / (maximum - minimum)));
            ctx.globalAlpha = Number(settings.panelOpacity ?? 0.86);
            ctx.fillStyle = settings.panelColor || "#111a22";
            ctx.beginPath();
            ctx.arc(cx, cy, radius * 1.30, 0, Math.PI * 2);
            ctx.fill();
            ctx.globalAlpha = 1;
            ctx.lineWidth = Math.max(frame.sceneScale, radius * 0.025);
            ctx.strokeStyle = settings.rimColor || "#8895a3";
            ctx.beginPath();
            ctx.arc(cx, cy, radius * 1.30, 0, Math.PI * 2);
            ctx.stroke();
            ctx.strokeStyle = settings.dialColor || "#f2f5f7";
            ctx.fillStyle = settings.dialColor || "#f2f5f7";
            ctx.textAlign = "center";
            ctx.textBaseline = "middle";
            ctx.font = "700 " + (frame.configuredFontSize() > 0
                ? frame.configuredFontSize() * frame.sceneScale
                : Math.max(9 * frame.sceneScale, radius * 0.18)) + "px " + frame.family;
            for (const ring of [0.92, 1.0]) {
                ctx.lineWidth = Math.max(2 * frame.sceneScale, radius * 0.035);
                ctx.beginPath();
                ctx.arc(cx, cy, radius * ring, Math.PI * 0.5, Math.PI * 2);
                ctx.stroke();
            }
            for (let step = 0; step <= steps; ++step) {
                const angle = Math.PI * 0.5 + Math.PI * 1.5 * step / steps;
                const highRpm = step / steps >= 0.78;
                ctx.strokeStyle = highRpm ? (settings.warningColor || "#e14b4b") : (settings.dialColor || "#f2f5f7");
                ctx.fillStyle = ctx.strokeStyle;
                ctx.lineWidth = Math.max(frame.sceneScale, radius * 0.025);
                ctx.beginPath();
                ctx.moveTo(cx + Math.cos(angle) * radius * 1.03, cy + Math.sin(angle) * radius * 1.03);
                ctx.lineTo(cx + Math.cos(angle) * radius * 1.14, cy + Math.sin(angle) * radius * 1.14);
                ctx.stroke();
                ctx.fillText(String(Math.round((minimum + (maximum - minimum) * step / steps) / 1000)), cx + Math.cos(angle) * radius * 1.35, cy + Math.sin(angle) * radius * 1.35);
            }
            if (hasValue) {
                const angle = Math.PI * 0.5 + Math.PI * 1.5 * progress;
                ctx.strokeStyle = settings.needleColor || "#e32636";
                ctx.lineWidth = Math.max(3 * frame.sceneScale, radius * 0.05);
                ctx.beginPath();
                ctx.moveTo(cx - Math.cos(angle) * radius * 0.13, cy - Math.sin(angle) * radius * 0.13);
                ctx.lineTo(cx + Math.cos(angle) * radius * 0.86, cy + Math.sin(angle) * radius * 0.86);
                ctx.stroke();
                ctx.fillStyle = settings.dialColor || "#f2f5f7";
                ctx.beginPath();
                ctx.arc(cx, cy, Math.max(5 * frame.sceneScale, radius * 0.12), 0, Math.PI * 2);
                ctx.fill();
            }
            const plateWidth = radius * 1.15;
            const plateHeight = Math.max(18 * frame.sceneScale, radius * 0.32);
            const plateX = cx - plateWidth / 2;
            const plateY = cy + radius * 0.52;
            ctx.globalAlpha = Number(settings.backgroundOpacity ?? 0.90);
            roundedPath(ctx, plateX, plateY, plateWidth, plateHeight, Math.max(3 * frame.sceneScale, plateHeight * 0.18));
            ctx.fillStyle = settings.valuePlateColor || "#111a22";
            ctx.fill();
            ctx.globalAlpha = 1;
            ctx.strokeStyle = settings.rimColor || "#8895a3";
            ctx.lineWidth = Math.max(1, frame.sceneScale * 0.75);
            roundedPath(ctx, plateX, plateY, plateWidth, plateHeight, Math.max(3 * frame.sceneScale, plateHeight * 0.18));
            ctx.stroke();
            ctx.fillStyle = settings.dialColor || "#f2f5f7";
            ctx.font = "700 " + Math.max(11 * frame.sceneScale, plateHeight * 0.52) + "px " + frame.family;
            ctx.fillText(hasValue ? Math.round(value).toString() : "—", cx, plateY + plateHeight * 0.53);
            ctx.font = "600 " + Math.max(8 * frame.sceneScale, plateHeight * 0.28) + "px " + frame.family;
            ctx.fillStyle = "#c0c8d0";
            ctx.fillText(settings.label || "RPM", cx, plateY + plateHeight * 0.84);
        }
    }
    Connections {
        target: frame.widgetModel
        ignoreUnknownSignals: true
        function onRevisionChanged() {
            retroTachometerCanvas.requestPaint();
        }
    }
}
