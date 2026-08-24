import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property var frame: parent.frame
    anchors.fill: parent
    property var rawValue: frame.raw("source", "")
    property var adjustedValue: frame.adjusted(rawValue)
    property bool hasValue: adjustedValue !== undefined && adjustedValue !== null
        && Number.isFinite(Number(adjustedValue))
    property string formattedValue: {
        if (!hasValue)
            return frame.widgetSettings.fallbackText ?? "—";
        return (frame.widgetSettings.prefix || "")
            + Number(adjustedValue).toFixed(Number(frame.widgetSettings.decimals ?? 1))
            + (frame.widgetSettings.suffix || "");
    }

    Rectangle {
        anchors.fill: parent
        color: frame.widgetSettings.panelColor || "#f4f4f4"
    }
    Column {
        anchors.centerIn: parent
        width: parent.width * 0.9
        spacing: 1 * frame.sceneScale
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: frame.widgetSettings.label || frame.widgetSettings.source || "VALUE"
            color: frame.widgetSettings.labelColor || "#3d433c"
            font.family: frame.family
            font.weight: Font.Bold
            font.pixelSize: Math.max(8 * frame.sceneScale, parent.parent.height * 0.18)
            elide: Text.ElideRight
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: parent.parent.formattedValue
            color: frame.widgetSettings.valueColor || "#111111"
            font.family: frame.family
            font.weight: Font.Bold
            font.pixelSize: frame.configuredFontSize() > 0
                ? frame.configuredFontSize() * frame.sceneScale
                : Math.max(10 * frame.sceneScale, Math.min(parent.parent.height * 0.38, parent.parent.width * 0.24))
            elide: Text.ElideRight
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: frame.widgetSettings.showUnit ?? true
            text: frame.widgetSettings.unit || ""
            color: frame.widgetSettings.labelColor || "#3d433c"
            font.family: frame.family
            font.weight: Font.DemiBold
            font.pixelSize: Math.max(8 * frame.sceneScale, parent.parent.height * 0.15)
        }
    }
}
