import QtQuick

// Shared surface for the modern motorsport-broadcast widgets. Content remains
// a sibling of these layers so text and indicators retain full opacity.
Item {
    id: root

    property var frame: null
    property color panelColor: frame ? frame.panel : "#16232d"
    property color panelBorder: frame ? frame.panelBorder : "#96a8b8"
    property color accentColor: frame ? frame.accent : "transparent"
    property bool showAccent: false
    property bool showBackground: frame ? (frame.widgetSettings.showBackground ?? true) : true
    property bool showBorder: frame ? (frame.widgetSettings.showBorder ?? true) : true
    property real backgroundOpacity: frame ? Number(frame.widgetSettings.backgroundOpacity ?? 0.78) : 0.78
    property real borderOpacity: frame ? Number(frame.widgetSettings.borderOpacity ?? 0.45) : 0.45
    property real borderWidth: frame ? Number(frame.widgetSettings.borderWidth ?? 1) * frame.sceneScale : 1
    property real cornerRadius: frame ? frame.panelRadius : 12
    property real innerPadding: frame ? frame.pad : 10
    property string stackPosition: frame ? String(frame.widgetSettings.stackPosition ?? "single") : "single"
    readonly property bool squareTop: stackPosition === "middle" || stackPosition === "bottom"
    readonly property bool squareBottom: stackPosition === "middle" || stackPosition === "top"

    // Canonical scene-pixel typography roles. Explicit widget font-size
    // settings still take precedence in the renderers that support them.
    readonly property real panelLabelSize: (frame ? 15 * frame.labelScale * frame.sceneScale : 15)
    readonly property real panelValueSize: (frame ? 56 * frame.valueScale * frame.sceneScale : 56)
    readonly property real panelUnitSize: (frame ? 15 * frame.labelScale * frame.sceneScale : 15)
    readonly property real compactValueSize: (frame ? 26 * frame.valueScale * frame.sceneScale : 26)

    Rectangle {
        anchors.fill: parent
        visible: root.showBackground
        radius: root.cornerRadius
        topLeftRadius: root.squareTop ? 0 : root.cornerRadius
        topRightRadius: root.squareTop ? 0 : root.cornerRadius
        bottomLeftRadius: root.squareBottom ? 0 : root.cornerRadius
        bottomRightRadius: root.squareBottom ? 0 : root.cornerRadius
        color: root.panelColor
        opacity: root.backgroundOpacity
    }
    Rectangle {
        anchors.fill: parent
        visible: root.showBorder
        radius: root.cornerRadius
        topLeftRadius: root.squareTop ? 0 : root.cornerRadius
        topRightRadius: root.squareTop ? 0 : root.cornerRadius
        bottomLeftRadius: root.squareBottom ? 0 : root.cornerRadius
        bottomRightRadius: root.squareBottom ? 0 : root.cornerRadius
        color: "transparent"
        border.width: root.borderWidth
        border.color: root.panelBorder
        opacity: root.borderOpacity
    }
    Rectangle {
        visible: root.showAccent
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: Math.max(2, root.cornerRadius * 0.18)
        width: Math.max(2, root.borderWidth * 2)
        radius: width / 2
        color: root.accentColor
        opacity: 0.9
    }
}
