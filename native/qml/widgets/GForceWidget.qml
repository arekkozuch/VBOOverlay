import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property var frame: parent.frame
    anchors.fill: parent
    property real lateral: Number(frame.raw("lateralSource", "lateralAcceleration") || 0)
    property real longitudinal: Number(frame.raw("longitudinalSource", "longitudinalAcceleration") || 0)
    property real range: Math.max(0.1, Number(frame.widgetSettings.gRange ?? 2))
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height - 20) * 0.76
        height: width
        radius: width / 2
        color: "transparent"
        border.color: frame.widgetSettings.gridColor || "#566477"
    }
    Rectangle {
        anchors.centerIn: parent
        width: 1
        height: parent.height * 0.68
        color: frame.widgetSettings.gridColor || "#566477"
    }
    Rectangle {
        anchors.centerIn: parent
        width: parent.width * 0.68
        height: 1
        color: frame.widgetSettings.gridColor || "#566477"
    }
    Rectangle {
        property real dot: Number(frame.widgetSettings.dotSize ?? 12)
        width: dot
        height: dot
        radius: dot / 2
        color: frame.accent
        x: parent.width / 2 - width / 2 + Math.max(-1, Math.min(1, parent.lateral / parent.range)) * parent.width * 0.28
        y: parent.height / 2 - height / 2 - Math.max(-1, Math.min(1, parent.longitudinal / parent.range)) * parent.height * 0.28
    }
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        visible: frame.widgetSettings.showCombined ?? true
        text: Math.sqrt(parent.lateral * parent.lateral + parent.longitudinal * parent.longitudinal).toFixed(Number(frame.widgetSettings.decimals ?? 2)) + " g"
        color: frame.primary
        font.family: frame.family
        font.weight: Font.DemiBold
        font.pixelSize: 10 * frame.labelScale
    }
}

