import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property var frame: parent.frame
    anchors.fill: parent
    property var topValue: frame.raw("topSource", "")
    property var bottomValue: frame.raw("bottomSource", "")
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: parent.height * 0.48
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#ffffff"
            }
            GradientStop {
                position: 0.55
                color: "#bfc0c7"
            }
            GradientStop {
                position: 1
                color: "#f7f7f7"
            }
        }
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: parent.height * 0.48
        anchors.bottom: parent.bottom
        color: "#050505"
    }
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        height: parent.height * 0.48
        verticalAlignment: Text.AlignVCenter
        text: parent.topValue !== undefined && parent.topValue !== null && Number.isFinite(Number(parent.topValue)) ? Number(parent.topValue).toFixed(Number(frame.widgetSettings.topDecimals ?? 0)) : (frame.widgetSettings.topText || "—")
        color: frame.widgetSettings.topColor || "#111111"
        font.family: frame.family
        font.weight: Font.Bold
        font.pixelSize: Math.max(9 * frame.sceneScale, parent.height * 0.25)
    }
    Label {
        anchors.right: parent.right
        anchors.rightMargin: parent.width * 0.06
        anchors.bottom: parent.bottom
        height: parent.height * 0.52
        verticalAlignment: Text.AlignVCenter
        text: parent.bottomValue !== undefined && parent.bottomValue !== null && Number.isFinite(Number(parent.bottomValue)) ? Number(parent.bottomValue).toFixed(Number(frame.widgetSettings.bottomDecimals ?? 1)) : (frame.widgetSettings.bottomText || "—")
        color: frame.widgetSettings.bottomColor || "#f4f4f4"
        font.family: frame.family
        font.weight: Font.Bold
        font.pixelSize: Math.max(9 * frame.sceneScale, parent.height * 0.26)
    }
}
