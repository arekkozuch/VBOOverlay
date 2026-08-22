import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Image {
    property var frame: parent.frame
    anchors.centerIn: parent
    width: parent.width * Math.max(0.1, Math.min(1, Number(frame.widgetSettings.logoScale ?? 1)))
    height: parent.height * Math.max(0.1, Math.min(1, Number(frame.widgetSettings.logoScale ?? 1)))
    source: "qrc:/flappedear/resources/branding/app-logo.png"
    sourceSize.width: 512
    sourceSize.height: 512
    fillMode: Image.PreserveAspectFit
    opacity: Number(frame.widgetSettings.logoOpacity ?? 0.85)
    mipmap: true
}

