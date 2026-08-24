import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Column {
    property var frame: parent.frame
    anchors.centerIn: parent
    width: parent.width
    spacing: 1 * frame.sceneScale
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        visible: frame.widgetSettings.showIcon ?? true
        text: "♡"
        color: frame.accent
        font.pixelSize: Math.min(22 * frame.sceneScale, frame.height * 0.20)
    }
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        text: frame.widgetSettings.label || "HR"
        color: frame.secondary
        font.family: frame.family
        font.weight: Font.DemiBold
        font.pixelSize: Math.max(11, 13 * frame.labelScale) * frame.sceneScale
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
            : Math.max(26 * frame.sceneScale, Math.min(42 * frame.sceneScale, frame.height * 0.31)) * frame.valueScale
    }
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        visible: frame.widgetSettings.showUnit ?? true
        text: frame.widgetSettings.unit || "bpm"
        color: frame.secondary
        font.family: frame.family
        font.weight: Font.DemiBold
        font.pixelSize: Math.max(10, 12 * frame.labelScale) * frame.sceneScale
    }
}
