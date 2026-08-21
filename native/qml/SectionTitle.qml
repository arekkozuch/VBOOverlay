import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property string text: ""
    Layout.fillWidth: true
    implicitHeight: 28

    RowLayout {
        anchors.fill: parent
        spacing: 8
        Label {
            text: root.text.toUpperCase()
            color: "#778596"
            font.family: "Helvetica Neue"
            font.pixelSize: 10
            font.weight: Font.DemiBold
            font.letterSpacing: 1.2
        }
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#202a36"
        }
    }
}
