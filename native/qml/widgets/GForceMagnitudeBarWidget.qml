import QtQuick
import QtQuick.Controls

Item {
    id: root
    property var frame: parent.frame
    anchors.fill: parent
    readonly property real maxG: {
        const candidate = Number(frame.widgetSettings.maxG ?? 1.5);
        return Number.isFinite(candidate) && candidate > 0 ? candidate : 1.5;
    }
    GForceData {
        id: gForce
        frame: root.frame
    }
    Rectangle {
        anchors.fill: parent
        radius: Number(root.frame.widgetSettings.barRadius ?? 5) * root.frame.sceneScale
        color: root.frame.widgetSettings.barBackgroundColor || "#24303d"
        opacity: Number(root.frame.widgetSettings.backgroundOpacity ?? 0.82)
        Rectangle {
            width: gForce.hasValue ? parent.width * Math.min(1, gForce.combinedG / root.maxG) : 0
            height: parent.height
            radius: parent.radius
            color: root.frame.widgetSettings.barColor || root.frame.accent
        }
    }
    Label {
        anchors.left: parent.left
        anchors.leftMargin: Math.max(6 * root.frame.sceneScale, parent.width * 0.05)
        anchors.verticalCenter: parent.verticalCenter
        visible: root.frame.widgetSettings.showLabel ?? true
        text: root.frame.widgetSettings.labelText || "G-Force"
        color: root.frame.widgetSettings.textColor || root.frame.primary
        font.family: root.frame.family
        font.weight: Font.DemiBold
        font.pixelSize: root.frame.configuredFontSize() > 0
            ? root.frame.configuredFontSize() * root.frame.sceneScale
            : Math.max(9 * root.frame.sceneScale, parent.height * 0.45)
        elide: Text.ElideRight
    }
    Label {
        anchors.right: parent.right
        anchors.rightMargin: Math.max(6 * root.frame.sceneScale, parent.width * 0.05)
        anchors.verticalCenter: parent.verticalCenter
        visible: root.frame.widgetSettings.showValue ?? true
        text: gForce.hasValue ? gForce.combinedG.toFixed(Number(root.frame.widgetSettings.decimals ?? 2)) : "—"
        color: root.frame.widgetSettings.textColor || root.frame.primary
        font.family: root.frame.family
        font.weight: Font.Bold
        font.pixelSize: root.frame.configuredFontSize() > 0
            ? root.frame.configuredFontSize() * root.frame.sceneScale
            : Math.max(9 * root.frame.sceneScale, parent.height * 0.48)
    }
}
