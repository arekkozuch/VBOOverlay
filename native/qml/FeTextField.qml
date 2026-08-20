import QtQuick
import QtQuick.Controls

TextField {
    id: control
    implicitHeight: 36
    color: "#e8edf4"
    placeholderTextColor: "#596575"
    selectionColor: "#55e6a5"
    selectedTextColor: "#07140f"
    leftPadding: 11
    rightPadding: 11
    font.family: "Helvetica Neue"
    font.pixelSize: 12
    background: Rectangle {
        radius: 7
        color: control.enabled ? "#0d131b" : "#10151c"
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? "#55e6a5" : "#273342"
    }
}
