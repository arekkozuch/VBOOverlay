import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    property var frame: parent.frame
    anchors.fill: parent
    spacing: 4
    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Column {
            Layout.alignment: Qt.AlignVCenter
            Label {
                text: frame.widgetSettings.label || "ENGINE"
                color: frame.secondary
                font.family: frame.family
                font.pixelSize: 9 * frame.labelScale
                font.letterSpacing: 1.2
            }
            Label {
                text: frame.numberText(frame.raw("source", "rpm"), 1)
                color: Number(frame.adjusted(frame.raw("source", "rpm")) || 0) >= Number(frame.widgetSettings.warningValue ?? 6500) ? "#ff6978" : frame.primary
                font.family: frame.family
                font.weight: frame.weight
                font.pixelSize: Math.min(34, frame.height * 0.42) * frame.valueScale
            }
        }
        Label {
            visible: frame.widgetSettings.showUnit ?? true
            text: frame.widgetSettings.unit || "RPM"
            color: frame.accent
            font.family: frame.family
            font.pixelSize: 10 * frame.labelScale
        }
    }
    Rectangle {
        visible: frame.widgetSettings.showBar ?? true
        Layout.fillWidth: true
        height: 5
        radius: 3
        color: "#24303d"
        Rectangle {
            property real rpmValue: Number(frame.adjusted(frame.raw("source", "rpm")) || 0)
            width: parent.width * Math.max(0, Math.min(1, rpmValue / Math.max(1, Number(frame.widgetSettings.maxValue ?? 8000))))
            height: parent.height
            radius: 3
            color: rpmValue >= Number(frame.widgetSettings.warningValue ?? 6500) ? "#ff5b63" : frame.accent
        }
    }
}

