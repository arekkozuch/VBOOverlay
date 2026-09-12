pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property real mediaDuration
    signal seekRequested(real milliseconds)

    color: "#090e14"
    border.color: "#202a36"

    property real durationSeconds: Math.max(0.001, mediaDuration > 0 ? mediaDuration / 1000 : appController.telemetryDuration)
    property var plotColors: ["#55e6a5", "#42a5ff", "#ffb84d", "#ff647c"]

    function seekAt(ratio) {
        const bounded = Math.max(0, Math.min(1, ratio));
        if (mediaDuration > 0)
            seekRequested(bounded * mediaDuration);
        else
            appController.playbackTime = bounded * durationSeconds;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            visible: appController.eventRuns.length > 0
            spacing: 8
            Label {
                text: appController.eventName
                Layout.fillWidth: true
                elide: Text.ElideRight
                color: "#dce4ee"
                font.pixelSize: 12
            }
            Label {
                text: qsTr("Active run")
                color: "#8d9aaa"
                font.pixelSize: 11
            }
            FeComboBox {
                id: runPicker
                objectName: "eventRunPicker"
                Layout.preferredWidth: 230
                implicitHeight: 30
                model: appController.eventRuns.map(run => run.name)
                currentIndex: appController.eventRuns.findIndex(run => run.id === appController.activeRunId)
                enabled: !appController.projectLoading && !appController.exporting
                    && !appController.recoveryPending && appController.pendingDestructiveAction === ""
                onActivated: index => {
                    appController.selectEventRun(appController.eventRuns[index].id);
                    // Restore the authoritative selection even if a guarded switch was refused.
                    currentIndex = Qt.binding(() => appController.eventRuns.findIndex(run => run.id === appController.activeRunId));
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                text: qsTr("TELEMETRY ANALYSIS")
                color: "#8d9aaa"
                font.pixelSize: 9
                font.weight: Font.DemiBold
                font.letterSpacing: 1.2
            }
            Label {
                text: qsTr("%1 synchronized channels").arg(appController.analysisChannels.length)
                color: "#536172"
                font.pixelSize: 9
            }
            Item {
                Layout.fillWidth: true
            }
            FeComboBox {
                id: channelPicker
                Layout.preferredWidth: 210
                implicitHeight: 30
                model: appController.channelNames.filter(channel => appController.analysisChannels.indexOf(channel) < 0)
            }
            FeButton {
                compact: true
                text: qsTr("Add channel")
                enabled: channelPicker.count > 0 && appController.analysisChannels.length < 4
                onClicked: appController.toggleAnalysisChannel(channelPicker.currentText)
            }
        }

        LapTimingPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            Layout.minimumHeight: 118
            onSeekRequested: milliseconds => root.seekRequested(milliseconds)
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: "#070b10"
            border.color: "#1c2631"
            clip: true

            SplitView {
                id: chartSplit
                anchors.fill: parent
                anchors.margins: 6
                orientation: Qt.Vertical
                handle: Rectangle {
                    implicitHeight: 5
                    color: SplitHandle.pressed ? "#55e6a5" : SplitHandle.hovered ? "#334556" : "#14202a"
                }

                Repeater {
                    model: appController.analysisChannels

                    Item {
                        id: chartRow
                        required property int index
                        required property var modelData
                        SplitView.fillWidth: true
                        SplitView.preferredHeight: Math.max(54, chartSplit.height / Math.max(1, appController.analysisChannels.length))
                        SplitView.minimumHeight: 42
                        property string channelName: String(modelData)
                        property color lineColor: root.plotColors[index % root.plotColors.length]
                        property var series: {
                            appController.syncOffset;
                            appController.timeScale;
                            appController.telemetryDuration;
                            return appController.telemetrySeries(channelName, 0, root.durationSeconds, Math.max(100, Math.round(width * 1.5)));
                        }
                        property bool hasData: (series.segments || []).length > 0

                        Item {
                            anchors.fill: parent

                            Column {
                                id: channelInfo
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.min(112, parent.width * 0.18)
                                spacing: 0
                                Label {
                                    width: channelInfo.width
                                    text: chartRow.channelName
                                    color: chartRow.lineColor
                                    font.pixelSize: 9
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideMiddle
                                }
                                Row {
                                    spacing: 5
                                    Label {
                                        text: {
                                            appController.playbackTime;
                                            return appController.valueText(chartRow.channelName, 2);
                                        }
                                        color: "#e4ebf3"
                                        font.family: "Menlo"
                                        font.pixelSize: 11
                                    }
                                    Label {
                                        text: chartRow.series.unit || ""
                                        color: "#687789"
                                        font.pixelSize: 8
                                    }
                                }
                            }
                            Label {
                                anchors.left: parent.left
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 1
                                text: "×"
                                color: removeMouse.containsMouse ? "#ff8090" : "#647386"
                                font.pixelSize: 11
                                MouseArea {
                                    id: removeMouse
                                    anchors.fill: parent
                                    anchors.margins: -5
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: appController.toggleAnalysisChannel(chartRow.channelName)
                                }
                            }

                            Item {
                                id: plotArea
                                anchors.left: channelInfo.right
                                anchors.leftMargin: 8
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom

                                Canvas {
                                    id: chartCanvas
                                    anchors.fill: parent
                                    property var plotSeries: chartRow.series
                                    onPlotSeriesChanged: requestPaint()
                                    onWidthChanged: requestPaint()
                                    onHeightChanged: requestPaint()
                                    onPaint: {
                                        const context = getContext("2d");
                                        context.clearRect(0, 0, width, height);
                                        context.strokeStyle = "#18232e";
                                        context.lineWidth = 1;
                                        for (let grid = 1; grid < 4; ++grid) {
                                            const x = width * grid / 4;
                                            context.beginPath();
                                            context.moveTo(x, 0);
                                            context.lineTo(x, height);
                                            context.stroke();
                                        }
                                        const segments = plotSeries.segments || [];
                                        if (segments.length === 0)
                                            return;
                                        const rawLow = Number(plotSeries.minimum);
                                        const rawHigh = Number(plotSeries.maximum);
                                        const padding = rawHigh === rawLow ? Math.max(0.5, Math.abs(rawLow) * 0.05) : 0;
                                        const low = rawLow - padding;
                                        const high = rawHigh + padding;
                                        const span = Math.max(0.000001, high - low);
                                        context.strokeStyle = chartRow.lineColor;
                                        context.lineWidth = 1.6;
                                        context.lineJoin = "round";
                                        for (let segmentIndex = 0; segmentIndex < segments.length; ++segmentIndex) {
                                            const points = segments[segmentIndex];
                                            if (points.length === 0)
                                                continue;
                                            context.beginPath();
                                            for (let pointIndex = 0; pointIndex < points.length; ++pointIndex) {
                                                const x = Math.max(0, Math.min(width, Number(points[pointIndex].x) * width));
                                                const y = height - 3 - (Number(points[pointIndex].y) - low) / span * Math.max(1, height - 6);
                                                if (pointIndex === 0)
                                                    context.moveTo(x, y);
                                                else
                                                    context.lineTo(x, y);
                                            }
                                            context.stroke();
                                            if (points.length === 1) {
                                                context.fillStyle = chartRow.lineColor;
                                                context.beginPath();
                                                context.arc(Math.max(0, Math.min(width, Number(points[0].x) * width)),
                                                            height - 3 - (Number(points[0].y) - low) / span * Math.max(1, height - 6),
                                                            2, 0, Math.PI * 2);
                                                context.fill();
                                            }
                                        }
                                    }
                                }
                                Label {
                                    anchors.centerIn: parent
                                    visible: !chartRow.hasData
                                    text: qsTr("No data in selected range")
                                    color: "#657386"
                                    font.pixelSize: 10
                                }
                                Rectangle {
                                    x: Math.max(0, Math.min(parent.width - width, appController.playbackTime / root.durationSeconds * parent.width))
                                    width: 1
                                    height: parent.height
                                    color: "#f3f6fa"
                                    opacity: 0.8
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onPressed: mouse => root.seekAt(mouse.x / width)
                                    onPositionChanged: mouse => {
                                        if (pressed)
                                            root.seekAt(mouse.x / width);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: appController.analysisChannels.length === 0
                text: appController.channelNames.length ? qsTr("Add a telemetry channel to begin analysis") : qsTr("Open a VBO file to inspect telemetry")
                color: "#657386"
                font.pixelSize: 11
            }
        }
    }
}
