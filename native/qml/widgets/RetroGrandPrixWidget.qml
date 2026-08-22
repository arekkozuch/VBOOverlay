import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property var frame: parent.frame
    anchors.fill: parent
    property var rpmRaw: frame.raw("rpmSource", "rpm")
    property var speedRaw: frame.raw("speedSource", "speed")
    property bool hasRpm: rpmRaw !== undefined && rpmRaw !== null && Number.isFinite(Number(rpmRaw))
    property bool hasSpeed: speedRaw !== undefined && speedRaw !== null && Number.isFinite(Number(speedRaw))
    property real rpmValue: hasRpm ? Number(rpmRaw) : 0
    property real speedValue: hasSpeed ? Number(speedRaw) : 0
    property real gearValue: Number(frame.raw("gearSource", "gear"))
    property real throttleValue: Number(frame.raw("throttleSource", "throttle") || 0)
    property real brakeValue: Number(frame.raw("brakeSource", "brake") || 0)
    property var timingValue: frame.raw("timingSource", "")
    Canvas {
        id: retroCanvas
        anchors.fill: parent
        property bool hasSpeed: parent.hasSpeed
        property real rpmValue: parent.rpmValue
        property real speedValue: parent.speedValue
        property real gearValue: parent.gearValue
        property real throttleValue: parent.throttleValue
        property real brakeValue: parent.brakeValue
        property var timingValue: parent.timingValue
        onHasSpeedChanged: requestPaint()
        onRpmValueChanged: requestPaint()
        onSpeedValueChanged: requestPaint()
        onGearValueChanged: requestPaint()
        onThrottleValueChanged: requestPaint()
        onBrakeValueChanged: requestPaint()
        onTimingValueChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.save();
            ctx.scale(width / 440, height / 420);
            const settings = frame.widgetSettings;
            const family = settings.fontFamily || "Arial Narrow";
            const white = settings.dialColor || "#f4f4f4";
            const panel = settings.panelColor || "#111111";
            const panelOpacity = Number(settings.panelOpacity ?? 0.58);
            const rpmMin = Number(settings.rpmMin ?? 0);
            const rpmMax = Math.max(rpmMin + 1, Number(settings.rpmMax ?? 8000));
            const rpmProgress = Math.max(0, Math.min(1, (rpmValue - rpmMin) / (rpmMax - rpmMin)));

            // Translucent round backing and the double white tachometer ring.
            ctx.globalAlpha = panelOpacity;
            ctx.fillStyle = panel;
            ctx.beginPath();
            ctx.arc(160, 142, 126, 0, Math.PI * 2);
            ctx.fill();
            ctx.globalAlpha = 1;
            ctx.strokeStyle = white;
            ctx.lineWidth = 4;
            for (const radius of [89, 98]) {
                ctx.beginPath();
                ctx.arc(160, 142, radius, Math.PI * 0.50, Math.PI * 2.0);
                ctx.stroke();
            }
            ctx.textAlign = "center";
            ctx.textBaseline = "middle";
            ctx.fillStyle = white;
            ctx.font = "700 18px " + family;
            const rpmSteps = Math.max(4, Math.min(16, Math.round((rpmMax - rpmMin) / 1000)));
            for (let step = 0; step <= rpmSteps; ++step) {
                const angle = Math.PI * 0.5 + Math.PI * 1.5 * step / rpmSteps;
                const major = true;
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.moveTo(160 + Math.cos(angle) * 101, 142 + Math.sin(angle) * 101);
                ctx.lineTo(160 + Math.cos(angle) * 111, 142 + Math.sin(angle) * 111);
                ctx.stroke();
                const label = Math.round((rpmMin + (rpmMax - rpmMin) * step / rpmSteps) / 1000);
                ctx.fillText(String(label), 160 + Math.cos(angle) * 125, 142 + Math.sin(angle) * 125);
            }
            const needleAngle = Math.PI * 0.5 + Math.PI * 1.5 * rpmProgress;
            ctx.strokeStyle = settings.needleColor || "#d73737";
            ctx.lineWidth = 5;
            ctx.beginPath();
            ctx.moveTo(160 - Math.cos(needleAngle) * 15, 142 - Math.sin(needleAngle) * 15);
            ctx.lineTo(160 + Math.cos(needleAngle) * 86, 142 + Math.sin(needleAngle) * 86);
            ctx.stroke();
            ctx.fillStyle = white;
            ctx.beginPath();
            ctx.arc(160, 142, 12, 0, Math.PI * 2);
            ctx.fill();

            // Gear and pedal-state stack.
            const boxX = 212;
            const boxW = 132;
            const boxH = 34;
            ctx.globalAlpha = 0.96;
            ctx.fillStyle = "#f4f4f4";
            ctx.fillRect(boxX, 143, boxW, boxH);
            ctx.fillStyle = "#111111";
            ctx.font = "700 20px " + family;
            const gear = Number.isFinite(gearValue) ? Math.round(gearValue).toString() : "—";
            ctx.fillText((settings.gearLabel || "Gear") + "  " + gear, boxX + boxW / 2, 160);
            const throttleProgress = Math.max(0, Math.min(1, throttleValue / 100));
            ctx.globalAlpha = 0.45 + throttleProgress * 0.55;
            ctx.fillStyle = settings.throttleColor || "#00c839";
            ctx.fillRect(boxX, 177, boxW, boxH);
            ctx.globalAlpha = 1;
            ctx.fillStyle = white;
            ctx.fillText(settings.throttleLabel || "Throttle", boxX + boxW / 2, 194);
            const brakeProgress = Math.max(0, Math.min(1, brakeValue / 100));
            ctx.globalAlpha = 0.5 + brakeProgress * 0.5;
            ctx.fillStyle = brakeProgress > 0.03 ? (settings.brakeActiveColor || "#d23737") : (settings.brakeColor || "#575244");
            ctx.fillRect(boxX, 211, boxW, boxH);
            ctx.globalAlpha = 1;
            ctx.fillStyle = white;
            ctx.fillText(settings.brakeLabel || "Brake", boxX + boxW / 2, 228);

            // Segmented speed arc.
            const speedMax = Math.max(1, Number(settings.speedMax ?? 360));
            const speedProgress = Math.max(0, Math.min(1, speedValue / speedMax));
            const speedStart = Math.PI * 0.94;
            const speedEnd = Math.PI * 1.79;
            const segments = 19;
            ctx.lineWidth = 13;
            for (let segment = 0; segment < segments; ++segment) {
                const progress = segment / (segments - 1);
                const a0 = speedStart + (speedEnd - speedStart) * segment / segments;
                const a1 = speedStart + (speedEnd - speedStart) * (segment + 0.72) / segments;
                if (progress <= speedProgress) {
                    ctx.strokeStyle = progress < 0.58 ? (settings.speedLowColor || "#00bd31") : (progress < 0.82 ? (settings.speedMidColor || "#f2e920") : (settings.speedHighColor || "#ff9124"));
                    ctx.globalAlpha = 1;
                } else {
                    ctx.strokeStyle = "#d8d8d8";
                    ctx.globalAlpha = 0.24;
                }
                ctx.beginPath();
                ctx.arc(115, 318, 120, a0, a1);
                ctx.stroke();
            }
            ctx.globalAlpha = 1;
            ctx.fillStyle = white;
            ctx.font = "700 17px " + family;
            ctx.textAlign = "left";
            ctx.fillText((retroCanvas.hasSpeed ? Math.round(speedValue).toString() : "—") + " Km/h", 18, 354);
            ctx.textAlign = "center";
            ctx.fillText("200", 196, 356);
            ctx.fillText("260", 252, 320);
            ctx.fillText("320", 281, 275);
            ctx.fillText("360", 292, 239);

            // Driver and timing lower-third.
            const plateX = 205;
            const plateY = 348;
            const plateW = 220;
            const plateH = 62;
            const gradient = ctx.createLinearGradient(plateX, plateY, plateX, plateY + 28);
            gradient.addColorStop(0, "#ffffff");
            gradient.addColorStop(0.55, "#c8c8cf");
            gradient.addColorStop(1, "#f7f7f7");
            ctx.fillStyle = gradient;
            ctx.fillRect(plateX, plateY, plateW, 29);
            ctx.fillStyle = "#050505";
            ctx.fillRect(plateX, plateY + 29, plateW, plateH - 29);
            ctx.font = "700 19px " + family;
            ctx.fillStyle = "#111111";
            ctx.fillText(settings.driverName || "DRIVER", plateX + plateW / 2, plateY + 15);
            ctx.fillStyle = white;
            ctx.textAlign = "right";
            const timing = timingValue !== undefined && timingValue !== null && Number.isFinite(Number(timingValue))
                ? Number(timingValue).toFixed(Number(settings.timingDecimals ?? 1))
                : (settings.timingText || "—");
            ctx.fillText(timing, plateX + plateW - 14, plateY + 46);
            ctx.restore();
        }
    }
    Connections {
        target: frame.widgetModel
        function onRevisionChanged() {
            retroCanvas.requestPaint();
        }
    }
}
