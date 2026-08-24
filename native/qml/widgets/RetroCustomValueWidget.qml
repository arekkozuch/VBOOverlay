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
    property string displayUnit: frame.widgetSettings.unit || ""
    property string formattedValue: {
        if (!hasValue)
            return frame.widgetSettings.fallbackText ?? "—";
        return (frame.widgetSettings.prefix || "")
            + Number(adjustedValue).toFixed(Number(frame.widgetSettings.decimals ?? 1))
            + (frame.widgetSettings.suffix || "");
    }

    // This renderer owns its panel so independently placed values still form
    // the same visual family when stacked directly against one another.
    Rectangle {
        anchors.fill: parent
        radius: 12 * frame.sceneScale
        color: frame.panel
        opacity: Number(frame.widgetSettings.backgroundOpacity ?? 0.86)
        border.width: Math.max(1, frame.sceneScale)
        border.color: frame.panelBorder
        border.opacity: 0.55
    }
    Row {
        anchors.fill: parent
        spacing: 8 * frame.sceneScale
        Label {
            width: parent.width * 0.46
            anchors.verticalCenter: parent.verticalCenter
            text: frame.widgetSettings.label || frame.widgetSettings.source || "VALUE"
            color: frame.widgetSettings.labelColor || frame.secondary
            font.family: frame.family
            font.weight: Font.DemiBold
            font.pixelSize: Math.max(12 * frame.sceneScale, parent.parent.height * 0.22)
            elide: Text.ElideRight
        }
        Row {
            width: parent.width - parent.children[0].width - parent.spacing
            height: parent.height
            spacing: 4 * frame.sceneScale
            layoutDirection: Qt.RightToLeft
            Label {
                anchors.verticalCenter: parent.verticalCenter
                visible: parent.parent.displayUnit !== "" ? frame.widgetSettings.showUnit !== false : false
                text: parent.parent.displayUnit
                color: frame.widgetSettings.labelColor || frame.secondary
                font.family: frame.family
                font.weight: Font.DemiBold
                font.pixelSize: Math.max(11 * frame.sceneScale, parent.parent.height * 0.20)
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: parent.parent.parent.formattedValue
                color: frame.widgetSettings.valueColor || frame.primary
                font.family: frame.family
                font.weight: Font.Bold
                font.pixelSize: frame.configuredFontSize() > 0
                    ? frame.configuredFontSize() * frame.sceneScale
                    : Math.max(16 * frame.sceneScale, Math.min(parent.parent.height * 0.46, parent.parent.width * 0.20))
                elide: Text.ElideLeft
            }
        }
    }
}
