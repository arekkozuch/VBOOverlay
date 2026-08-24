import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property var frame: parent.frame
    anchors.fill: parent
    property var rawValue: frame.raw("source", "")
    property var formattedValue: {
        const adjusted = frame.adjusted(rawValue);
        if (adjusted === undefined || adjusted === null || !Number.isFinite(Number(adjusted)))
            return frame.widgetSettings.fallbackText ?? "—";
        return (frame.widgetSettings.prefix || "") + Number(adjusted).toFixed(Number(frame.widgetSettings.decimals ?? 0)) + (frame.widgetSettings.suffix || "");
    }
    Rectangle {
        anchors.fill: parent
        color: frame.widgetSettings.panelColor || "#f4f4f4"
    }
    Label {
        anchors.centerIn: parent
        text: {
            const label = frame.widgetSettings.label ?? "Gear";
            const unit = (frame.widgetSettings.showUnit ?? true) ? (frame.widgetSettings.unit || "") : "";
            return [label, parent.formattedValue, unit].filter(part => String(part).length > 0).join("  ");
        }
        color: frame.widgetSettings.valueColor || "#111111"
        font.family: frame.family
        font.weight: Font.Bold
        font.pixelSize: frame.configuredFontSize() > 0
            ? frame.configuredFontSize() * frame.sceneScale
            : Math.max(9 * frame.sceneScale, Math.min(parent.height * 0.52, parent.width * 0.12))
    }
}
