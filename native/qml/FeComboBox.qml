import QtQuick
import QtQuick.Controls

ComboBox {
    id: control
    implicitHeight: 36
    leftPadding: 11
    rightPadding: 30
    hoverEnabled: true

    contentItem: Text {
        text: control.displayText
        color: control.enabled ? "#e8edf4" : "#596575"
        font.family: "Helvetica Neue"
        font.pixelSize: 12
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Text {
        x: control.width - width - 11
        anchors.verticalCenter: parent.verticalCenter
        text: "⌄"
        color: "#7d8a9a"
        font.pixelSize: 15
    }
    background: Rectangle {
        radius: 7
        color: control.hovered ? "#131b25" : "#0d131b"
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? "#55e6a5" : "#273342"
    }
    delegate: ItemDelegate {
        required property int index
        required property var modelData
        highlighted: control.highlightedIndex === index
        width: control.width
        height: 34
        contentItem: Text {
            text: parent.modelData
            color: parent.highlighted ? "#07140f" : "#dce4ee"
            font.family: "Helvetica Neue"
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideMiddle
        }
        background: Rectangle {
            color: parent.highlighted ? "#55e6a5" : parent.hovered ? "#1a2430" : "#101720"
        }
    }
    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 280)
        padding: 4
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle {
            radius: 8
            color: "#101720"
            border.color: "#2a3746"
        }
    }
}
