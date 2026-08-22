import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    property var frame: parent.frame
    anchors.fill: parent
    spacing: 7 * frame.sceneScale
    Repeater {
        model: [
            {
                "labelKey": "acceleratorLabel",
                "sourceKey": "acceleratorSource",
                "fallback": "throttle",
                "colorKey": "acceleratorColor",
                "minKey": "acceleratorMin",
                "maxKey": "acceleratorMax"
            },
            {
                "labelKey": "brakeLabel",
                "sourceKey": "brakeSource",
                "fallback": "brake",
                "colorKey": "brakeColor",
                "minKey": "brakeMin",
                "maxKey": "brakeMax"
            }
        ]
        ColumnLayout {
            required property var modelData
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 3 * frame.sceneScale
            property var pedalRaw: frame.raw(modelData.sourceKey, modelData.fallback)
            property real pedalValue: Number(pedalRaw || 0)
            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: frame.widgetSettings[modelData.labelKey] || modelData.fallback.toUpperCase()
                    color: frame.secondary
                    font.family: frame.family
                    font.pixelSize: 9 * frame.labelScale * frame.sceneScale
                    font.letterSpacing: 0.8 * frame.sceneScale
                }
                Label {
                    visible: frame.widgetSettings.showValues ?? true
                    text: parent.parent.pedalRaw === undefined ? "—" : parent.parent.pedalValue.toFixed(Number(frame.widgetSettings.decimals ?? 0)) + "%"
                    color: frame.primary
                    font.family: frame.family
                    font.weight: Font.DemiBold
                    font.pixelSize: 10 * frame.labelScale * frame.sceneScale
                }
            }
            Rectangle {
                Layout.fillWidth: true
                height: 8 * frame.sceneScale
                radius: Number(frame.widgetSettings.barRadius ?? 5) * frame.sceneScale
                color: "#24303d"
                Rectangle {
                    property real low: Number(frame.widgetSettings[modelData.minKey] ?? 0)
                    property real high: Number(frame.widgetSettings[modelData.maxKey] ?? 100)
                    width: parent.width * Math.max(0, Math.min(1, (parent.parent.pedalValue - low) / Math.max(0.001, high - low)))
                    height: parent.height
                    radius: parent.radius
                    color: frame.widgetSettings[modelData.colorKey] || frame.accent
                }
            }
        }
    }
}
