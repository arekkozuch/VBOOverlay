import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

RowLayout {
    id: root
    property string colorValue: "#ffffff"
    signal edited(string value)
    spacing: 6

    Rectangle {
        width: 32
        height: 32
        radius: 7
        color: root.colorValue
        border.color: "#526071"
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: colorDialog.open()
        }
    }
    FeTextField {
        Layout.fillWidth: true
        text: root.colorValue
        onEditingFinished: root.edited(text)
    }
    ColorDialog {
        id: colorDialog
        selectedColor: root.colorValue
        onAccepted: root.edited(selectedColor.toString())
    }
}
