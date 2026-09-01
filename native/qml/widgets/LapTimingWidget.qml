import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var frame: parent.frame
    property var timing: {
        frame.renderContext.time;
        return frame.renderContext.lapTiming;
    }
    readonly property int timingDecimals: Number(frame.widgetSettings.timingDecimals ?? 1)
    readonly property int deltaDecimals: Number(frame.widgetSettings.deltaDecimals ?? 2)
    readonly property real deltaRange: Math.max(1, Number(frame.widgetSettings.deltaRangeSeconds ?? 10))
    readonly property real liveDelta: Number(timing.liveDeltaSeconds)
    readonly property bool hasLiveDelta: Number.isFinite(liveDelta)
    readonly property color aheadColor: frame.widgetSettings.accentColor || "#20d05a"
    readonly property color behindColor: frame.widgetSettings.accentColor2 || "#ef4f5f"
    readonly property color tileColor: frame.widgetSettings.backgroundColor || "#343941"
    readonly property real tileOpacity: (frame.widgetSettings.showBackground ?? true)
        ? Number(frame.widgetSettings.backgroundOpacity ?? 0.76) : 0
    readonly property color tileBorder: Qt.rgba(
        frame.panelBorder.r, frame.panelBorder.g, frame.panelBorder.b,
        Number(frame.widgetSettings.borderOpacity ?? 0.45))
    readonly property real tileBorderWidth: (frame.widgetSettings.showBorder ?? false)
        ? Number(frame.widgetSettings.borderWidth ?? 1) * frame.sceneScale : 0
    readonly property color deltaColor: !hasLiveDelta ? frame.secondary
        : liveDelta <= 0 ? aheadColor : behindColor

    function formatTime(seconds) {
        const value = Number(seconds);
        if (!Number.isFinite(value) || value < 0)
            return "—:—." + "—".repeat(Math.max(1, timingDecimals));
        const minutes = Math.floor(value / 60);
        const remainder = value - minutes * 60;
        const width = timingDecimals > 0 ? 3 + timingDecimals : 2;
        return minutes + ":" + remainder.toFixed(timingDecimals).padStart(width, "0");
    }

    function formatDelta(seconds) {
        const value = Number(seconds);
        if (!Number.isFinite(value))
            return "—";
        const rounded = Math.abs(value) < 0.5 * Math.pow(10, -deltaDecimals) ? 0 : value;
        return (rounded > 0 ? "+" : "") + rounded.toFixed(deltaDecimals);
    }

    RowLayout {
        anchors.fill: parent
        spacing: Math.max(5, 12 * root.frame.sceneScale)

        Rectangle {
            visible: root.frame.widgetSettings.showBest ?? true
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            radius: Number(root.frame.widgetSettings.cornerRadius ?? 12) * root.frame.sceneScale
            color: Qt.rgba(root.tileColor.r, root.tileColor.g, root.tileColor.b, root.tileOpacity)
            border.width: root.tileBorderWidth
            border.color: root.tileBorder

            Label {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: 12 * root.frame.sceneScale
                anchors.topMargin: 7 * root.frame.sceneScale
                text: qsTr("Best")
                color: root.frame.primary
                font.family: root.frame.family
                font.pixelSize: 24 * root.frame.labelScale * root.frame.sceneScale
                font.weight: Font.Medium
            }
            Label {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: 12 * root.frame.sceneScale
                anchors.bottomMargin: 13 * root.frame.sceneScale
                text: root.timing.bestLapNumber ?? "—"
                color: root.frame.primary
                font.family: root.frame.family
                font.pixelSize: 24 * root.frame.labelScale * root.frame.sceneScale
                font.weight: Font.DemiBold
            }
            Label {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 12 * root.frame.sceneScale
                anchors.bottomMargin: 7 * root.frame.sceneScale
                text: root.formatTime(root.timing.bestLapSeconds)
                color: root.frame.primary
                font.family: root.frame.family
                font.pixelSize: 50 * root.frame.valueScale * root.frame.sceneScale
                font.weight: Font.Medium
            }
        }

        Rectangle {
            visible: root.frame.widgetSettings.showCurrent ?? true
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            radius: Number(root.frame.widgetSettings.cornerRadius ?? 12) * root.frame.sceneScale
            color: Qt.rgba(root.tileColor.r, root.tileColor.g, root.tileColor.b, root.tileOpacity)
            border.width: root.tileBorderWidth
            border.color: root.tileBorder

            Label {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: 12 * root.frame.sceneScale
                anchors.topMargin: 7 * root.frame.sceneScale
                text: qsTr("Current")
                color: root.frame.primary
                font.family: root.frame.family
                font.pixelSize: 24 * root.frame.labelScale * root.frame.sceneScale
                font.weight: Font.Medium
            }
            Label {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: 12 * root.frame.sceneScale
                anchors.bottomMargin: 13 * root.frame.sceneScale
                text: root.timing.currentLapNumber ?? "—"
                color: root.frame.primary
                font.family: root.frame.family
                font.pixelSize: 24 * root.frame.labelScale * root.frame.sceneScale
                font.weight: Font.DemiBold
            }
            Label {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 12 * root.frame.sceneScale
                anchors.bottomMargin: 7 * root.frame.sceneScale
                text: root.timing.state === "waiting" ? qsTr("READY")
                    : root.formatTime(root.timing.currentElapsedSeconds)
                color: root.frame.primary
                font.family: root.frame.family
                font.pixelSize: root.timing.state === "waiting"
                    ? 31 * root.frame.valueScale * root.frame.sceneScale
                    : 50 * root.frame.valueScale * root.frame.sceneScale
                font.weight: Font.Medium
            }
        }

        Rectangle {
            visible: root.frame.widgetSettings.showDelta ?? true
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            radius: Number(root.frame.widgetSettings.cornerRadius ?? 12) * root.frame.sceneScale
            color: Qt.rgba(root.tileColor.r, root.tileColor.g, root.tileColor.b, root.tileOpacity)
            border.width: root.tileBorderWidth
            border.color: root.tileBorder

            Item {
                id: deltaGauge
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 10 * root.frame.sceneScale
                anchors.rightMargin: 10 * root.frame.sceneScale
                anchors.topMargin: 8 * root.frame.sceneScale
                height: parent.height * 0.34
                readonly property real centerX: width / 2
                readonly property real magnitude: root.hasLiveDelta
                    ? Math.min(1, Math.abs(root.liveDelta) / root.deltaRange) : 0

                Rectangle {
                    z: 2
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    height: Math.max(1, 2 * root.frame.sceneScale)
                    color: root.frame.primary
                    opacity: 0.9
                }
                Repeater {
                    model: 5
                    Rectangle {
                        z: 2
                        required property int index
                        x: index * (deltaGauge.width - width) / 4
                        anchors.verticalCenter: deltaGauge.verticalCenter
                        width: Math.max(1, 2 * root.frame.sceneScale)
                        height: index === 2 ? deltaGauge.height * 0.72 : deltaGauge.height * 0.42
                        color: root.frame.primary
                        opacity: 0.9
                    }
                }
                Rectangle {
                    z: 1
                    visible: root.hasLiveDelta
                    x: root.liveDelta <= 0 ? deltaGauge.centerX
                                           : deltaGauge.centerX - width
                    anchors.verticalCenter: parent.verticalCenter
                    width: deltaGauge.width * 0.5 * deltaGauge.magnitude
                    height: parent.height * 0.74
                    color: root.deltaColor
                    opacity: 0.92
                }
            }

            Label {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 12 * root.frame.sceneScale
                anchors.bottomMargin: 7 * root.frame.sceneScale
                text: root.formatDelta(root.timing.liveDeltaSeconds)
                color: root.hasLiveDelta ? root.frame.primary : root.frame.secondary
                font.family: root.frame.family
                font.pixelSize: 50 * root.frame.valueScale * root.frame.sceneScale
                font.weight: Font.Medium
            }
        }
    }
}
