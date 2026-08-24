import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    property var frame: parent.frame
    anchors.fill: parent
    spacing: 8 * frame.sceneScale
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
            spacing: 5 * frame.sceneScale
            property var pedalRaw: frame.raw(modelData.sourceKey, modelData.fallback)
            property bool hasValue: pedalRaw !== undefined && pedalRaw !== null && Number.isFinite(Number(pedalRaw))
            property real pedalValue: hasValue ? Number(pedalRaw) : 0
            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: frame.widgetSettings[modelData.labelKey] || (modelData.fallback === "throttle" ? "Throttle" : "Brake")
                    color: frame.secondary
                    font.family: frame.family
                    font.weight: Font.DemiBold
                    font.pixelSize: Math.max(11, 13 * frame.labelScale) * frame.sceneScale
                }
                Label {
                    visible: frame.widgetSettings.showValues ?? true
                    text: parent.parent.hasValue ? parent.parent.pedalValue.toFixed(Number(frame.widgetSettings.decimals ?? 0)) + "%" : "—"
                    color: frame.primary
                    font.family: frame.family
                    font.weight: Font.DemiBold
                    font.pixelSize: Math.max(11, 13 * frame.labelScale) * frame.sceneScale
                }
            }
            Rectangle {
                Layout.fillWidth: true
                height: Math.max(6 * frame.sceneScale, parent.parent.height * 0.16)
                radius: Number(frame.widgetSettings.barRadius ?? 5) * frame.sceneScale
                color: frame.neutralTrack
                Rectangle {
                    property real low: Number(frame.widgetSettings[modelData.minKey] ?? 0)
                    property real high: Number(frame.widgetSettings[modelData.maxKey] ?? 100)
                    width: parent.parent.hasValue ? parent.width * Math.max(0, Math.min(1, (parent.parent.pedalValue - low) / Math.max(0.001, high - low))) : 0
                    height: parent.height
                    radius: parent.radius
                    color: frame.widgetSettings[modelData.colorKey]
                        || (modelData.fallback === "throttle" ? frame.throttle : frame.brake)
                }
            }
        }
    }
}
