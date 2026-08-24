import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    property var frame: parent.frame
    anchors.centerIn: parent
    spacing: 9 * frame.sceneScale
    Label {
        visible: frame.widgetSettings.showIcon ?? true
        text: "♥"
        color: frame.accent
        font.pixelSize: Math.min(30 * frame.sceneScale, frame.height * 0.38)
    }
    Column {
        Label {
            text: frame.widgetSettings.label || "HEART RATE"
            color: frame.secondary
            font.family: frame.family
            font.pixelSize: 9 * frame.labelScale * frame.sceneScale
            font.letterSpacing: frame.sceneScale
        }
        Row {
            spacing: 6 * frame.sceneScale
            Label {
                text: frame.numberText(frame.raw("source", "heartRate"), 1)
                color: frame.primary
                font.family: frame.family
                font.weight: frame.weight
                font.pixelSize: frame.configuredFontSize() > 0
                    ? frame.configuredFontSize() * frame.sceneScale
                    : Math.min(28 * frame.sceneScale, frame.height * 0.38) * frame.valueScale
            }
            Label {
                anchors.baseline: parent.children[0].baseline
                visible: frame.widgetSettings.showUnit ?? true
                text: frame.widgetSettings.unit || "BPM"
                color: frame.accent
                font.family: frame.family
                font.pixelSize: 9 * frame.labelScale * frame.sceneScale
            }
        }
    }
}
