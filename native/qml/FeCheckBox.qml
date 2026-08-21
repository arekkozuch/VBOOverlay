import QtQuick
import QtQuick.Controls

CheckBox {
    id: control
    implicitHeight: 28
    spacing: 7
    hoverEnabled: true
    indicator: Rectangle {
        x: control.leftPadding
        anchors.verticalCenter: parent.verticalCenter
        width: 17
        height: 17
        radius: 5
        color: control.checked ? "#55e6a5" : control.hovered ? "#17212c" : "#0d131b"
        border.color: control.checked ? "#55e6a5" : "#344253"
        Text {
            anchors.centerIn: parent
            text: "✓"
            visible: control.checked
            color: "#07140f"
            font.pixelSize: 11
            font.weight: Font.Bold
        }
    }
    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        color: control.enabled ? "#b8c3cf" : "#596575"
        font.family: "Helvetica Neue"
        font.pixelSize: 11
        verticalAlignment: Text.AlignVCenter
    }
}
