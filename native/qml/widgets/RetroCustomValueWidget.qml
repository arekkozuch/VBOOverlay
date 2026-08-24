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
        Label {
            width: parent.width * 0.46
            anchors.verticalCenter: parent.verticalCenter
            text: frame.widgetSettings.label || frame.widgetSettings.source || "VALUE"
            color: frame.widgetSettings.labelColor || frame.secondary
            font.family: frame.family
            font.weight: Font.DemiBold
            font.pixelSize: Math.min(panel.panelLabelSize, parent.parent.height * 0.34)
            elide: Text.ElideRight
        }
        Row {
            width: parent.width - parent.children[0].width - parent.spacing
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
}
