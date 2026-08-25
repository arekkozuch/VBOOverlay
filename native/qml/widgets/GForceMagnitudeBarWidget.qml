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
    TelemetryPanel {
        id: panel
        anchors.fill: parent
        frame: root.frame
        accentColor: root.frame.widgetSettings.barColor || root.frame.gForceAccent
    }
    Column {
        anchors.fill: parent
        anchors.margins: panel.innerPadding
        spacing: Math.max(5 * root.frame.sceneScale, parent.height * 0.08)
        Row {
            width: parent.width
            height: parent.height * 0.44
            Label {
                width: parent.width * 0.62
                anchors.verticalCenter: parent.verticalCenter
                visible: root.frame.widgetSettings.showLabel ?? true
                text: root.frame.widgetSettings.labelText || "G-Force"
                color: root.frame.widgetSettings.textColor || root.frame.secondary
                font.family: root.frame.family
                font.weight: Font.DemiBold
                font.pixelSize: root.frame.configuredFontSize() > 0
                    ? root.frame.configuredFontSize() * root.frame.sceneScale
                    : panel.panelLabelSize
                elide: Text.ElideRight
            }
            Label {
                width: parent.width * 0.38
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignRight
                visible: root.frame.widgetSettings.showValue ?? true
                text: gForce.hasValue ? gForce.combinedG.toFixed(Number(root.frame.widgetSettings.decimals ?? 2)) : "—"
                color: root.frame.widgetSettings.textColor || root.frame.primary
                font.family: root.frame.family
                font.weight: Font.Bold
                font.pixelSize: root.frame.configuredFontSize() > 0
                    ? root.frame.configuredFontSize() * root.frame.sceneScale
                    : panel.compactValueSize
            }
        }
        Rectangle {
            width: parent.width
            height: Math.max(11 * root.frame.sceneScale, parent.parent.height * 0.25)
            radius: Number(root.frame.widgetSettings.barRadius ?? 5) * root.frame.sceneScale
            color: root.frame.neutralTrack
            Rectangle {
                width: gForce.hasValue ? parent.width * Math.min(1, gForce.combinedG / root.maxG) : 0
                height: parent.height
                radius: parent.radius
                color: root.frame.widgetSettings.barColor || "#f5a623"
            }
        }
    }
}
