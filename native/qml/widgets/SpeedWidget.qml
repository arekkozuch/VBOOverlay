import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property var frame: parent.frame
    Column {
        anchors.centerIn: parent
        spacing: 0
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: frame.widgetSettings.label || "SPEED"
            color: frame.secondary
            font.family: frame.family
            font.weight: Font.DemiBold
            font.pixelSize: Math.max(8, 10 * frame.labelScale)
            font.letterSpacing: 1.5
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            property bool mph: frame.widgetSettings.unit === "mph"
            text: frame.numberText(frame.raw("source", "speed"), mph ? 0.621371 : 1)
            color: frame.primary
            font.family: frame.family
            font.weight: frame.weight
            font.pixelSize: Math.min(frame.width * 0.36, frame.height * 0.43) * frame.valueScale
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: frame.widgetSettings.showUnit ?? true
            text: frame.widgetSettings.unit || "km/h"
            color: frame.accent
            font.family: frame.family
            font.weight: Font.DemiBold
            font.pixelSize: Math.max(8, 11 * frame.labelScale)
        }
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 3
        radius: 2
        color: "#24303d"
        Rectangle {
            property real value: Number(frame.adjusted(frame.raw("source", "speed")) || 0)
            width: parent.width * Math.max(0, Math.min(1, (value - Number(frame.widgetSettings.minValue ?? 0)) / Math.max(1, Number(frame.widgetSettings.maxValue ?? 300) - Number(frame.widgetSettings.minValue ?? 0))))
            height: parent.height
            radius: 2
            color: frame.accent
        }
    }
    
}
