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
    readonly property bool showDetails: (frame.widgetSettings.showLast ?? true)
        || (frame.widgetSettings.showBest ?? true)
        || (frame.widgetSettings.showDelta ?? true)

    function formatTime(seconds) {
        const value = Number(seconds);
        if (!Number.isFinite(value) || value < 0)
            return "—:—.---";
        const minutes = Math.floor(value / 60);
        const remainder = value - minutes * 60;
        return minutes + ":" + remainder.toFixed(3).padStart(6, "0");
    }

    function formatDelta(seconds, isBest) {
        const value = Number(seconds);
        if (isBest || (Number.isFinite(value) && value <= 0.0005))
            return qsTr("BEST");
        return Number.isFinite(value) ? "+" + value.toFixed(3) : "—";
    }

    TelemetryPanel {
        id: panel
        anchors.fill: parent
        frame: root.frame
        showAccent: true
        accentColor: root.frame.widgetSettings.accentColor2 || "#42a5ff"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: panel.innerPadding
        spacing: Math.max(3, 4 * root.frame.sceneScale)

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8 * root.frame.sceneScale

            Label {
                Layout.preferredWidth: parent.width * 0.27
                text: root.timing.state === "running"
                    ? qsTr("LAP %1").arg(root.timing.currentLapNumber)
                    : root.timing.state === "finished" ? qsTr("SESSION") : qsTr("LAP —")
                color: root.frame.secondary
                font.family: root.frame.family
                font.weight: Font.DemiBold
                font.pixelSize: Math.max(10, 15 * root.frame.labelScale) * root.frame.sceneScale
                font.letterSpacing: 0.6 * root.frame.sceneScale
            }

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                text: root.timing.state === "running"
                    ? root.formatTime(root.timing.currentElapsedSeconds)
                    : root.timing.state === "waiting" ? qsTr("READY")
                    : root.timing.state === "finished" ? qsTr("FINISHED") : qsTr("NO LAP DATA")
                color: root.timing.state === "running" ? root.frame.primary : root.frame.secondary
                font.family: root.frame.family
                font.weight: Font.Bold
                font.pixelSize: root.frame.configuredFontSize() > 0
                    ? root.frame.configuredFontSize() * root.frame.sceneScale
                    : Math.max(18, 36 * root.frame.valueScale) * root.frame.sceneScale
                elide: Text.ElideRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(1, root.frame.sceneScale)
            visible: root.showDetails
            color: root.frame.widgetSettings.borderColor || root.frame.panelBorder
            opacity: 0.32
        }

        RowLayout {
            visible: root.showDetails
            Layout.fillWidth: true
            Layout.preferredHeight: parent.height * 0.34
            spacing: 8 * root.frame.sceneScale

            Column {
                visible: root.frame.widgetSettings.showLast ?? true
                Layout.fillWidth: true
                Label {
                    text: qsTr("LAST")
                    color: root.frame.secondary
                    font.family: root.frame.family
                    font.pixelSize: 10 * root.frame.labelScale * root.frame.sceneScale
                    font.weight: Font.DemiBold
                }
                Label {
                    text: root.formatTime(root.timing.lastLapSeconds)
                    color: root.frame.primary
                    font.family: root.frame.family
                    font.pixelSize: 15 * root.frame.valueScale * root.frame.sceneScale
                    font.weight: Font.DemiBold
                }
            }

            Column {
                visible: root.frame.widgetSettings.showBest ?? true
                Layout.fillWidth: true
                Label {
                    text: qsTr("BEST")
                    color: root.frame.secondary
                    font.family: root.frame.family
                    font.pixelSize: 10 * root.frame.labelScale * root.frame.sceneScale
                    font.weight: Font.DemiBold
                }
                Label {
                    text: root.formatTime(root.timing.bestLapSeconds)
                    color: root.frame.accent
                    font.family: root.frame.family
                    font.pixelSize: 15 * root.frame.valueScale * root.frame.sceneScale
                    font.weight: Font.DemiBold
                }
            }

            Column {
                visible: root.frame.widgetSettings.showDelta ?? true
                Layout.fillWidth: true
                Label {
                    text: qsTr("LAST Δ")
                    color: root.frame.secondary
                    font.family: root.frame.family
                    font.pixelSize: 10 * root.frame.labelScale * root.frame.sceneScale
                    font.weight: Font.DemiBold
                }
                Label {
                    text: root.formatDelta(root.timing.lastDeltaToBestSeconds,
                                           root.timing.lastLapIsBest ?? false)
                    color: root.timing.lastLapIsBest ? root.frame.accent
                                                     : (root.frame.widgetSettings.accentColor2 || "#42a5ff")
                    font.family: root.frame.family
                    font.pixelSize: 15 * root.frame.valueScale * root.frame.sceneScale
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
