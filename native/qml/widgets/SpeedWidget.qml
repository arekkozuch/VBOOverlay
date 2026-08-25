import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property var frame: parent.frame

    TelemetryPanel {
        id: panel
        anchors.fill: parent
        frame: parent.frame
    }
    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: panel.innerPadding
        anchors.rightMargin: panel.innerPadding
        anchors.verticalCenter: parent.verticalCenter
        spacing: 1 * frame.sceneScale
        Label {
            text: frame.widgetSettings.label || "Speed"
            color: frame.secondary
            font.family: frame.family
            font.weight: Font.DemiBold
            font.pixelSize: panel.panelLabelSize
            font.letterSpacing: 0.35 * frame.sceneScale
        }
        Label {
            property bool mph: frame.widgetSettings.unit === "mph"
            text: frame.numberText(frame.raw("source", "speed"), mph ? 0.621371 : 1)
            color: frame.primary
            font.family: frame.family
            font.weight: frame.weight
            font.pixelSize: frame.configuredFontSize() > 0
                ? frame.configuredFontSize() * frame.sceneScale
                : Math.max(34 * frame.sceneScale, Math.min(panel.panelValueSize, parent.height * 0.62))
        }
        Label {
            visible: frame.widgetSettings.showUnit ?? true
            text: frame.widgetSettings.unit || "km/h"
            color: frame.secondary
            font.family: frame.family
            font.weight: Font.DemiBold
            font.pixelSize: panel.panelUnitSize
        }
    }
}
