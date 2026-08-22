import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GridLayout {
    property var frame: parent.frame
    anchors.fill: parent
    columns: Math.max(1, Math.min(4, Number(frame.widgetSettings.columns ?? 4)))
    columnSpacing: 0
    rowSpacing: 0
    Repeater {
        model: 4
        Item {
            required property int index
            Layout.fillWidth: true
            Layout.fillHeight: true
            property int slot: index + 1
            property var slotRaw: frame.raw("source" + slot, ["speed", "rpm", "throttle", "brake"][index])
            Rectangle {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 1
                height: parent.height * 0.62
                visible: (frame.widgetSettings.showSeparators ?? true) && parent.index < 3
                color: frame.widgetSettings.separatorColor || "#314052"
            }
            Column {
                anchors.centerIn: parent
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: frame.widgetSettings["label" + parent.parent.slot] || "VALUE"
                    color: frame.secondary
                    font.family: frame.family
                    font.pixelSize: 8 * frame.labelScale
                    font.letterSpacing: 0.8
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: frame.slotText(parent.parent.slotRaw, frame.widgetSettings["decimals" + parent.parent.slot] ?? 0)
                    color: frame.primary
                    font.family: frame.family
                    font.weight: frame.weight
                    font.pixelSize: Math.min(22, frame.height * 0.25) * frame.valueScale
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: frame.widgetSettings["unit" + parent.parent.slot] || ""
                    color: frame.accent
                    font.family: frame.family
                    font.pixelSize: 8 * frame.labelScale
                }
            }
        }
    }
}

