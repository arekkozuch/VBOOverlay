import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property var frame: parent.frame
    anchors.fill: parent
    property var rawValue: frame.raw("source", "")
    property var adjustedValue: frame.adjusted(rawValue)
    property bool hasValue: adjustedValue !== undefined && adjustedValue !== null && Number.isFinite(Number(adjustedValue))
    property real value: hasValue ? Number(adjustedValue) : 0
    property real minimum: Number(frame.widgetSettings.minValue ?? 0)
    property real maximum: Math.max(minimum + 0.001, Number(frame.widgetSettings.maxValue ?? 100))
    property real progress: Math.max(0, Math.min(1, (value - minimum) / (maximum - minimum)))
    Rectangle {
        anchors.fill: parent
        color: frame.widgetSettings.emptyColor || "#3d433c"
        opacity: 0.88
    }
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width * parent.progress
        color: frame.widgetSettings.fillColor || "#00c839"
    }
    Label {
        anchors.centerIn: parent
        text: (frame.widgetSettings.label || "Pedal") + ((frame.widgetSettings.showValue ?? false) ? "  " + (parent.hasValue ? parent.value.toFixed(Number(frame.widgetSettings.decimals ?? 0)) + "%" : "—") : "")
        color: frame.primary
        font.family: frame.family
        font.weight: Font.Bold
        font.pixelSize: Math.max(9 * frame.sceneScale, Math.min(parent.height * 0.50, parent.width * 0.12))
    }
}
