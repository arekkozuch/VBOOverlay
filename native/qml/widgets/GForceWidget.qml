import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var frame: parent.frame
    anchors.fill: parent
    GForceData {
        id: gForce
        frame: root.frame
    }
    property real range: Math.max(0.1, Number(frame.widgetSettings.gRange ?? 2))
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height - 20 * frame.sceneScale) * 0.76
        height: width
        radius: width / 2
        color: "transparent"
        border.color: frame.widgetSettings.gridColor || "#566477"
    }
    Rectangle {
        anchors.centerIn: parent
        width: frame.sceneScale
        height: parent.height * 0.68
        color: frame.widgetSettings.gridColor || "#566477"
    }
    Rectangle {
        anchors.centerIn: parent
        width: parent.width * 0.68
        height: frame.sceneScale
        color: frame.widgetSettings.gridColor || "#566477"
    }
    Rectangle {
        property real dot: Number(frame.widgetSettings.dotSize ?? 12) * frame.sceneScale
        objectName: "gForceDot"
        width: dot
        height: dot
        radius: dot / 2
        color: frame.accent
        visible: gForce.hasValue
        x: parent.width / 2 - width / 2 + Math.max(-1, Math.min(1, gForce.lateral / parent.range)) * parent.width * 0.28
        // Braking (negative acceleration) moves up; acceleration moves down.
        y: parent.height / 2 - height / 2 + Math.max(-1, Math.min(1, gForce.longitudinal / parent.range)) * parent.height * 0.28
    }
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        visible: frame.widgetSettings.showCombined ?? true
        text: gForce.hasValue
            ? gForce.combinedG.toFixed(Number(frame.widgetSettings.decimals ?? 2)) + " g"
            : "—"
        color: frame.primary
        font.family: frame.family
        font.weight: Font.DemiBold
        font.pixelSize: 10 * frame.labelScale * frame.sceneScale
    }
}
