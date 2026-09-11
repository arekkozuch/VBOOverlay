import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: window
    property string message: ""
    title: qsTr("FlappedEar Telemetry")
    width: 520
    height: Math.max(200, content.implicitHeight + 48)
    visible: true
    color: "#171d26"

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 24
        spacing: 20
        Label {
            Layout.fillWidth: true
            text: window.message
            color: "#f1f5fa"
            wrapMode: Text.Wrap
        }
        Button {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Close")
            onClicked: window.close()
        }
    }
}
