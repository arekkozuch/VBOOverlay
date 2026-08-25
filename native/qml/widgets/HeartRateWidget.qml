import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property var frame: parent.frame
    anchors.fill: parent
    TelemetryPanel {
        id: panel
        anchors.fill: parent
        frame: parent.frame
        accentColor: frame.widgetSettings.accentColor || frame.accent
    }
    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: panel.innerPadding
        anchors.rightMargin: panel.innerPadding
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
        visible: frame.widgetSettings.showIcon ?? true
        text: "♡"
        color: frame.accent
            font.pixelSize: Math.min(20 * frame.sceneScale, parent.height * 0.17)
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
        text: frame.widgetSettings.label || "HR"
        color: frame.secondary
        font.family: frame.family
        font.weight: Font.DemiBold
            font.pixelSize: panel.panelLabelSize
            font.letterSpacing: 0.7 * frame.sceneScale
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
        text: frame.numberText(frame.raw("source", "heartRate"), 1)
        color: frame.primary
        font.family: frame.family
        font.weight: frame.weight
        font.pixelSize: frame.configuredFontSize() > 0
            ? frame.configuredFontSize() * frame.sceneScale
                : Math.max(32 * frame.sceneScale, Math.min(panel.panelValueSize, parent.height * 0.48))
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
        visible: frame.widgetSettings.showUnit ?? true
        text: frame.widgetSettings.unit || "bpm"
        color: frame.secondary
        font.family: frame.family
        font.weight: Font.DemiBold
            font.pixelSize: panel.panelUnitSize
        }
    }
}
