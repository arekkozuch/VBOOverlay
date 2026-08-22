import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Column {
    property var frame: parent.frame
    anchors.centerIn: parent
    spacing: 2
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        text: frame.widgetSettings.label || frame.widgetSettings.source || "VALUE"
        color: frame.secondary
        font.family: frame.family
        font.pixelSize: 9 * frame.labelScale
        font.letterSpacing: 1.2
    }
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        text: frame.numberText(frame.raw("source", ""), 1)
        color: frame.primary
        font.family: frame.family
        font.weight: frame.weight
        font.pixelSize: Math.min(38, frame.height * 0.36) * frame.valueScale
    }
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        visible: frame.widgetSettings.showUnit ?? true
        text: frame.widgetSettings.unit || ""
        color: frame.accent
        font.family: frame.family
        font.pixelSize: 10 * frame.labelScale
    }
}

