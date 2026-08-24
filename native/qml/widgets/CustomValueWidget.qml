import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Column {
    property var frame: parent.frame
    anchors.centerIn: parent
    spacing: 2 * frame.sceneScale
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        text: frame.widgetSettings.label || frame.widgetSettings.source || "VALUE"
        color: frame.secondary
        font.family: frame.family
        font.pixelSize: 9 * frame.labelScale * frame.sceneScale
        font.letterSpacing: 1.2 * frame.sceneScale
    }
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        text: {
            const adjusted = frame.adjusted(frame.raw("source", ""));
            if (adjusted === undefined || !Number.isFinite(Number(adjusted)))
                return frame.widgetSettings.fallbackText ?? "—";
            return (frame.widgetSettings.prefix || "")
                + Number(adjusted).toFixed(Number(frame.widgetSettings.decimals ?? 1))
                + (frame.widgetSettings.suffix || "");
        }
        color: frame.primary
        font.family: frame.family
        font.weight: frame.weight
        font.pixelSize: frame.configuredFontSize() > 0
            ? frame.configuredFontSize() * frame.sceneScale
            : Math.min(38 * frame.sceneScale, frame.height * 0.36) * frame.valueScale
    }
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        visible: frame.widgetSettings.showUnit ?? true
        text: frame.widgetSettings.unit || ""
        color: frame.accent
        font.family: frame.family
        font.pixelSize: 10 * frame.labelScale * frame.sceneScale
    }
}
