import QtQuick

Item {
    id: root
    property var frame: parent.frame
    anchors.fill: parent
    readonly property real maxG: {
        const candidate = Number(frame.widgetSettings.maxG ?? 1.5);
        return Number.isFinite(candidate) && candidate > 0 ? candidate : 1.5;
    }
    readonly property real ringStepG: {
        const candidate = Number(frame.widgetSettings.ringStepG ?? 0.25);
        return Number.isFinite(candidate) && candidate > 0 ? candidate : 0.25;
    }
    readonly property int ringCount: Math.max(1, Math.floor(maxG / ringStepG))
    readonly property real fieldPadding: Math.max(5 * frame.sceneScale, Math.min(width, height) * 0.05)
    readonly property real fieldDiameter: Math.max(1, Math.min(width, height) - fieldPadding * 2)
    readonly property real fieldRadius: fieldDiameter / 2

    GForceData {
        id: gForce
        frame: root.frame
    }

    Rectangle {
        anchors.centerIn: parent
        width: root.fieldDiameter
        height: width
        radius: width / 2
        color: root.frame.widgetSettings.radarBackgroundColor || "#111a22"
        opacity: Number(root.frame.widgetSettings.backgroundOpacity ?? 0.86)
        border.width: Math.max(1.25, root.frame.sceneScale * 1.25)
        border.color: root.frame.widgetSettings.gridColor || "#8895a3"
    }
    Repeater {
        model: root.ringCount - 1
        Rectangle {
            required property int index
            anchors.centerIn: parent
            readonly property real diameter: root.fieldDiameter * ((index + 1) * root.ringStepG / root.maxG)
            width: diameter
            height: diameter
            radius: width / 2
            color: "transparent"
            border.width: Math.max(1, root.frame.sceneScale)
            border.color: root.frame.widgetSettings.gridColor || "#8895a3"
            opacity: 0.42 + (index + 1) / Math.max(1, root.ringCount - 1) * 0.16
        }
    }
    Rectangle {
        visible: root.frame.widgetSettings.showCrosshair ?? true
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: root.fieldDiameter
        height: Math.max(1, root.frame.sceneScale)
        color: root.frame.widgetSettings.gridColor || "#8895a3"
        opacity: 0.24
    }
    Rectangle {
        visible: root.frame.widgetSettings.showCrosshair ?? true
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(1, root.frame.sceneScale)
        height: root.fieldDiameter
        color: root.frame.widgetSettings.gridColor || "#8895a3"
        opacity: 0.24
    }
    Rectangle {
        visible: root.frame.widgetSettings.showCenterBox ?? true
        anchors.centerIn: parent
        width: Math.max(4 * root.frame.sceneScale, root.fieldDiameter * 0.065)
        height: width
        color: "transparent"
        border.width: Math.max(1, root.frame.sceneScale * 0.8)
        border.color: root.frame.widgetSettings.gridColor || "#8895a3"
        opacity: 0.52
    }
    Rectangle {
        readonly property real dotDiameter: Math.max(5 * root.frame.sceneScale, root.fieldDiameter * 0.055)
        objectName: "gForceDot"
        readonly property real availableRadius: Math.max(0, root.fieldRadius - dotDiameter / 2)
        readonly property real ratio: gForce.hasValue ? Math.min(1, gForce.combinedG / root.maxG) : 0
        width: dotDiameter
        height: dotDiameter
        radius: width / 2
        visible: gForce.hasValue
        color: root.frame.widgetSettings.dotColor || "#f5a623"
        x: parent.width / 2 - width / 2 + (gForce.combinedG > 0 ? gForce.lateral / gForce.combinedG : 0) * ratio * availableRadius
        // Match the G ball: braking up, acceleration down; raw signs are retained.
        y: parent.height / 2 - height / 2 + (gForce.combinedG > 0 ? gForce.longitudinal / gForce.combinedG : 0) * ratio * availableRadius
    }
}
