import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var frame: parent.frame
    anchors.fill: parent
    property var rawValue: frame.raw("source", "")
    property var adjustedValue: frame.adjusted(rawValue)
    property bool hasValue: adjustedValue !== undefined && adjustedValue !== null
        && Number.isFinite(Number(adjustedValue))
    property string displayUnit: frame.widgetSettings.unit || ""
    property string displayIcon: frame.widgetSettings.icon || ""
    readonly property string temperatureIcon: {
        const label = String(frame.widgetSettings.label || "").toUpperCase();
        if (label === "OIL")
            return "oil";
        if (label === "ATF")
            return "transmission";
        if (label === "COOLANT")
            return "coolant";
        return "";
    }
    readonly property url temperatureIconSource: {
        switch (temperatureIcon) {
        case "oil": return Qt.resolvedUrl("../assets/change-car-oil-svgrepo-com.svg");
        case "transmission": return Qt.resolvedUrl("../assets/temperature-transmission.svg");
        case "coolant": return Qt.resolvedUrl("../assets/engine-coolant-svgrepo-com.svg");
        default: return "";
        }
    }
    property string formattedValue: {
        if (!hasValue)
            return frame.widgetSettings.fallbackText ?? "—";
        return (frame.widgetSettings.prefix || "")
            + Number(adjustedValue).toFixed(Number(frame.widgetSettings.decimals ?? 1))
            + (frame.widgetSettings.suffix || "");
    }

    // Independently placed values retain the exact same surface when stacked.
    TelemetryPanel {
        id: panel
        anchors.fill: parent
        frame: parent.frame
        panelColor: frame.widgetSettings.panelColor || frame.panel
    }
    Row {
        anchors.fill: parent
        anchors.margins: panel.innerPadding
        spacing: 8 * frame.sceneScale
        Item {
            id: iconSlot
            visible: root.displayIcon !== "" || root.temperatureIconSource.toString() !== ""
            width: visible ? 30 * frame.sceneScale : 0
            height: parent.height
            anchors.verticalCenter: parent.verticalCenter
            Image {
                anchors.fill: parent
                visible: root.temperatureIconSource.toString() !== ""
                source: root.temperatureIconSource
                fillMode: Image.PreserveAspectFit
                sourceSize.width: Math.round(width)
                sourceSize.height: Math.round(height)
            }
            Label {
                anchors.fill: parent
                visible: root.temperatureIconSource.toString() === ""
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: root.displayIcon
                color: frame.widgetSettings.labelColor || frame.secondary
                font.family: frame.family
                font.weight: Font.DemiBold
                font.pixelSize: Math.min(19 * frame.sceneScale, parent.height * 0.38)
            }
        }
        Label {
            id: valueLabel
            width: parent.width * (iconSlot.visible ? 0.42 : 0.50)
            anchors.verticalCenter: parent.verticalCenter
            text: frame.widgetSettings.label || frame.widgetSettings.source || "VALUE"
            color: frame.widgetSettings.labelColor || frame.secondary
            font.family: frame.family
            font.weight: Font.DemiBold
            font.pixelSize: Math.min(panel.panelLabelSize, parent.parent.height * 0.34)
            elide: Text.ElideRight
        }
        Row {
            width: parent.width - iconSlot.width - valueLabel.width
                - parent.spacing * (iconSlot.visible ? 2 : 1)
            height: parent.height
            spacing: 4 * frame.sceneScale
            layoutDirection: Qt.RightToLeft
            Label {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.displayUnit !== "" ? frame.widgetSettings.showUnit !== false : false
                text: root.displayUnit
                color: frame.widgetSettings.labelColor || frame.secondary
                font.family: frame.family
                font.weight: Font.DemiBold
                font.pixelSize: Math.min(panel.panelUnitSize, parent.parent.height * 0.31)
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: parent.parent.parent.formattedValue
                color: frame.widgetSettings.valueColor || frame.primary
                font.family: frame.family
                font.weight: Font.Bold
                font.pixelSize: frame.configuredFontSize() > 0
                    ? frame.configuredFontSize() * frame.sceneScale
                    : Math.max(16 * frame.sceneScale, Math.min(panel.compactValueSize, parent.parent.height * 0.56))
                elide: Text.ElideLeft
            }
        }
    }
    Rectangle {
        visible: (frame.widgetSettings.showSeparator ?? true)
            && (frame.widgetSettings.stackPosition === "middle"
                || frame.widgetSettings.stackPosition === "bottom")
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: panel.innerPadding
        anchors.rightMargin: panel.innerPadding
        height: Math.max(1, frame.sceneScale)
        color: frame.panelBorder
        opacity: 0.42
    }
}
