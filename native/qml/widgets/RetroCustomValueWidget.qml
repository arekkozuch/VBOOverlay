import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var frame: parent.frame
    anchors.fill: parent
    property var rawValue: frame.raw("source", "")
    property var adjustedValue: frame.adjusted(rawValue)
    property bool hasValue: adjustedValue !== undefined && adjustedValue !== null
        && Number.isFinite(Number(adjustedValue))
    property string displayUnit: frame.widgetSettings.unit || ""
    property string displayIcon: frame.widgetSettings.icon || ""
    readonly property string temperatureIcon: {
        const label = String(frame.widgetSettings.label || "").toUpperCase();
        if (label === "OIL")
            return "oil";
        if (label === "ATF")
            return "transmission";
        if (label === "COOLANT")
            return "coolant";
        return "";
    }
    property string formattedValue: {
        if (!hasValue)
            return frame.widgetSettings.fallbackText ?? "—";
        return (frame.widgetSettings.prefix || "")
            + Number(adjustedValue).toFixed(Number(frame.widgetSettings.decimals ?? 1))
            + (frame.widgetSettings.suffix || "");
    }

    // Independently placed values retain the exact same surface when stacked.
    TelemetryPanel {
        id: panel
        anchors.fill: parent
        frame: parent.frame
        panelColor: frame.widgetSettings.panelColor || frame.panel
    }
    Row {
        anchors.fill: parent
        anchors.margins: panel.innerPadding
        spacing: 8 * frame.sceneScale
        Item {
            id: iconSlot
            visible: root.displayIcon !== "" || root.temperatureIcon !== ""
            width: visible ? 24 * frame.sceneScale : 0
            height: parent.height
            anchors.verticalCenter: parent.verticalCenter
            Canvas {
                id: temperatureIconCanvas
                anchors.fill: parent
                visible: root.temperatureIcon !== ""
                onVisibleChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d");
                    ctx.reset();
                    const color = root.frame.widgetSettings.labelColor || root.frame.secondary;
                    const line = Math.max(1.15 * root.frame.sceneScale, width * 0.075);
                    ctx.strokeStyle = color;
                    ctx.fillStyle = color;
                    ctx.lineWidth = line;
                    ctx.lineCap = "round";
                    ctx.lineJoin = "round";

                    if (root.temperatureIcon === "oil") {
                        ctx.strokeRect(width * 0.20, height * 0.34, width * 0.46, height * 0.34);
                        ctx.beginPath();
                        ctx.arc(width * 0.30, height * 0.35, width * 0.12, Math.PI, Math.PI * 2);
                        ctx.stroke();
                        ctx.beginPath();
                        ctx.moveTo(width * 0.66, height * 0.42);
                        ctx.lineTo(width * 0.84, height * 0.42);
                        ctx.lineTo(width * 0.88, height * 0.54);
                        ctx.stroke();
                        ctx.beginPath();
                        ctx.moveTo(width * 0.35, height * 0.34);
                        ctx.lineTo(width * 0.35, height * 0.22);
                        ctx.lineTo(width * 0.54, height * 0.22);
                        ctx.stroke();
                    } else if (root.temperatureIcon === "transmission") {
                        const cx = width * 0.50;
                        const cy = height * 0.50;
                        const radius = Math.min(width, height) * 0.25;
                        for (let tooth = 0; tooth < 8; ++tooth) {
                            const angle = tooth * Math.PI / 4;
                            ctx.beginPath();
                            ctx.moveTo(cx + Math.cos(angle) * radius, cy + Math.sin(angle) * radius);
                            ctx.lineTo(cx + Math.cos(angle) * radius * 1.30,
                                       cy + Math.sin(angle) * radius * 1.30);
                            ctx.stroke();
                        }
                        ctx.beginPath();
                        ctx.arc(cx, cy, radius, 0, Math.PI * 2);
                        ctx.stroke();
                        ctx.beginPath();
                        ctx.arc(cx, cy, radius * 0.34, 0, Math.PI * 2);
                        ctx.stroke();
                    } else if (root.temperatureIcon === "coolant") {
                        const stemX = width * 0.46;
                        const bulbY = height * 0.58;
                        ctx.beginPath();
                        ctx.moveTo(stemX, height * 0.20);
                        ctx.lineTo(stemX, bulbY);
                        ctx.stroke();
                        ctx.beginPath();
                        ctx.arc(stemX, bulbY + height * 0.08, width * 0.12, 0, Math.PI * 2);
                        ctx.stroke();
                        ctx.beginPath();
                        ctx.moveTo(stemX, height * 0.28);
                        ctx.lineTo(stemX + width * 0.13, height * 0.28);
                        ctx.stroke();
                        for (let wave = 0; wave < 2; ++wave) {
                            const y = height * (0.76 + wave * 0.10);
                            ctx.beginPath();
                            ctx.moveTo(width * 0.15, y);
                            ctx.quadraticCurveTo(width * 0.28, y - height * 0.07, width * 0.41, y);
                            ctx.quadraticCurveTo(width * 0.54, y + height * 0.07, width * 0.67, y);
                            ctx.quadraticCurveTo(width * 0.80, y - height * 0.07, width * 0.90, y);
                            ctx.stroke();
                        }
                    }
                    ctx.lineCap = "butt";
                    ctx.lineJoin = "miter";
                }
            }
            Label {
                anchors.fill: parent
                visible: root.temperatureIcon === ""
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: root.displayIcon
                color: frame.widgetSettings.labelColor || frame.secondary
                font.family: frame.family
                font.weight: Font.DemiBold
                font.pixelSize: Math.min(19 * frame.sceneScale, parent.height * 0.38)
            }
        }
        Label {
            id: valueLabel
            width: parent.width * (iconSlot.visible ? 0.42 : 0.50)
            anchors.verticalCenter: parent.verticalCenter
            text: frame.widgetSettings.label || frame.widgetSettings.source || "VALUE"
            color: frame.widgetSettings.labelColor || frame.secondary
            font.family: frame.family
            font.weight: Font.DemiBold
            font.pixelSize: Math.min(panel.panelLabelSize, parent.parent.height * 0.34)
            elide: Text.ElideRight
        }
        Row {
            width: parent.width - iconSlot.width - valueLabel.width
                - parent.spacing * (iconSlot.visible ? 2 : 1)
            height: parent.height
            spacing: 4 * frame.sceneScale
            layoutDirection: Qt.RightToLeft
            Label {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.displayUnit !== "" ? frame.widgetSettings.showUnit !== false : false
                text: root.displayUnit
                color: frame.widgetSettings.labelColor || frame.secondary
                font.family: frame.family
                font.weight: Font.DemiBold
                font.pixelSize: Math.min(panel.panelUnitSize, parent.parent.height * 0.31)
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: parent.parent.parent.formattedValue
                color: frame.widgetSettings.valueColor || frame.primary
                font.family: frame.family
                font.weight: Font.Bold
                font.pixelSize: frame.configuredFontSize() > 0
                    ? frame.configuredFontSize() * frame.sceneScale
                    : Math.max(16 * frame.sceneScale, Math.min(panel.compactValueSize, parent.parent.height * 0.56))
                elide: Text.ElideLeft
            }
        }
    }
    Rectangle {
        visible: (frame.widgetSettings.showSeparator ?? true)
            && (frame.widgetSettings.stackPosition === "middle"
                || frame.widgetSettings.stackPosition === "bottom")
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: panel.innerPadding
        anchors.rightMargin: panel.innerPadding
        height: Math.max(1, frame.sceneScale)
        color: frame.panelBorder
        opacity: 0.42
    }
}
