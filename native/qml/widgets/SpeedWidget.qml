import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property var frame: parent.frame

    Column {
        anchors.fill: parent
        spacing: 2 * frame.sceneScale
        Label {
            text: frame.widgetSettings.label || "Speed"
            color: frame.secondary
            font.family: frame.family
            font.weight: Font.DemiBold
            font.pixelSize: Math.max(12, 14 * frame.labelScale) * frame.sceneScale
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
                : Math.max(30 * frame.sceneScale, Math.min(frame.width * 0.54, frame.height * 0.48)) * frame.valueScale
        }
        Label {
            visible: frame.widgetSettings.showUnit ?? true
            text: frame.widgetSettings.unit || "km/h"
            color: frame.secondary
            font.family: frame.family
            font.weight: Font.DemiBold
            font.pixelSize: Math.max(11, 13 * frame.labelScale) * frame.sceneScale
        }
    }
}
