import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool accent: false
    property bool danger: false
    property bool compact: false

    implicitHeight: compact ? 32 : 38
    implicitWidth: Math.max(72, contentItem.implicitWidth + 28)
    padding: 0
    hoverEnabled: true

    contentItem: Text {
        text: control.text
        color: !control.enabled ? "#596575" : control.accent ? "#07140f" : control.danger ? "#ff8090" : "#dce4ee"
        font.family: "Helvetica Neue"
        font.pixelSize: 12
        font.weight: control.accent ? Font.DemiBold : Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 8
        color: {
            if (!control.enabled)
                return "#131922";
            if (control.accent)
                return control.down ? "#3fc98d" : control.hovered ? "#6af0b4" : "#55e6a5";
            if (control.danger)
                return control.down ? "#3a1720" : control.hovered ? "#2e1820" : "#21171d";
            return control.down ? "#202b38" : control.hovered ? "#1c2632" : "#151d27";
        }
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? "#55e6a5" : control.danger ? "#5a2935" : "#2a3645"
    }
}
