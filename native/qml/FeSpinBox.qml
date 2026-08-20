import QtQuick
import QtQuick.Controls

SpinBox {
    id: control
    implicitHeight: 36
    editable: true
    contentItem: TextInput {
        z: 2
        text: control.textFromValue(control.value, control.locale)
        color: "#e8edf4"
        selectionColor: "#55e6a5"
        selectedTextColor: "#07140f"
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        font.family: "Helvetica Neue"
        font.pixelSize: 12
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
    }
    up.indicator: Rectangle {
        x: parent.width - width
        height: parent.height
        width: 30
        color: control.up.pressed ? "#22303d" : "transparent"
        Text {
            anchors.centerIn: parent
            text: "+"
            color: "#8d9aaa"
            font.pixelSize: 14
        }
    }
    down.indicator: Rectangle {
        x: 0
        height: parent.height
        width: 30
        color: control.down.pressed ? "#22303d" : "transparent"
        Text {
            anchors.centerIn: parent
            text: "−"
            color: "#8d9aaa"
            font.pixelSize: 14
        }
    }
    background: Rectangle {
        radius: 7
        color: "#0d131b"
        border.color: control.activeFocus ? "#55e6a5" : "#273342"
        border.width: control.activeFocus ? 2 : 1
    }
}
