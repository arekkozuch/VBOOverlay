pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property bool lapDetail: false
    required property real mediaDuration
    signal seekRequested(real milliseconds)

    color: "#090e14"
    border.color: "#202a36"

    property real durationSeconds: Math.max(0.001, mediaDuration > 0 ? mediaDuration / 1000 : appController.telemetryDuration)
    readonly property var visibleChannels: lapDetail ? appController.outingLapChannels : appController.analysisChannels
    readonly property real rangeStart: lapDetail ? Number(appController.selectedOutingLap.startTime || 0) : 0
    readonly property real rangeEnd: lapDetail ? Number(appController.selectedOutingLap.endTime || 1) : durationSeconds
    readonly property real cursorTime: lapDetail ? appController.outingLapCursor : appController.playbackTime
    property var plotColors: ["#55e6a5", "#42a5ff", "#ffb84d", "#ff647c"]

    readonly property var availableChannels: lapDetail ? appController.outingLapAvailableChannels : appController.channelNames

    function setChannels(channels) {
        if (lapDetail) appController.outingLapChannels = channels;
        else appController.analysisChannels = channels;
    }
    function toggleChannel(channel) {
        const channels = visibleChannels.slice();
        const index = channels.indexOf(channel);
        if (index >= 0) channels.splice(index, 1);
        else if (channel && channels.length < 4) channels.push(channel);
        setChannels(channels);
    }
    function replaceChannel(previous, next) {
        const channels = visibleChannels.slice();
        const index = channels.indexOf(previous);
        if (index >= 0 && (previous === next || channels.indexOf(next) < 0)) {
            channels[index] = next;
            setChannels(channels);
        }
    }
    // Negative longitudinal G is braking: draw it upward without negating data.
    function graphY(value, low, high, height, brakingUp) {
        const fraction = (value - low) / Math.max(0.000001, high - low);
        return 3 + (brakingUp ? fraction : 1 - fraction) * Math.max(1, height - 6);
    }

    function seekAt(ratio) {
        const bounded = Math.max(0, Math.min(1, ratio));
        if (lapDetail)
            appController.outingLapCursor = rangeStart + bounded * (rangeEnd - rangeStart);
        else if (mediaDuration > 0)
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
            visible: !root.lapDetail && appController.eventRuns.length > 0
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

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: qsTr("TELEMETRY ANALYSIS")
                    color: "#8d9aaa"
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("%1/4 channels").arg(root.visibleChannels.length)
                    color: "#687789"
                    font.pixelSize: 9
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                FeComboBox {
                    id: channelPicker
                    objectName: "analysisChannelPicker"
                    popupMinimumWidth: 320
                    wrapPopupText: true
                    Layout.fillWidth: true
                    Layout.minimumWidth: 80
                    implicitHeight: 30
                    model: root.availableChannels.filter(channel => root.visibleChannels.indexOf(channel) < 0)
                    enabled: count > 0 && root.visibleChannels.length < 4
                }
                FeButton {
                    objectName: "analysisAddChannel"
                    compact: true
                    text: qsTr("Add channel")
                    enabled: channelPicker.enabled
                    onClicked: root.toggleChannel(channelPicker.currentText)
                }
            }
            Label {
                Layout.fillWidth: true
                visible: root.visibleChannels.length === 0
                text: qsTr("Choose a recorded channel above to add a graph.")
                wrapMode: Text.WordWrap
                color: "#8d9aaa"
                font.pixelSize: 10
            }
        }

        LapTimingPanel {
            visible: !root.lapDetail
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
                    model: root.visibleChannels

                    Item {
                        id: chartRow
                        required property int index
                        required property var modelData
                        SplitView.fillWidth: true
                        SplitView.preferredHeight: Math.max(54, chartSplit.height / Math.max(1, root.visibleChannels.length))
                        SplitView.minimumHeight: 42
                        property string channelName: String(modelData)
                        property color lineColor: root.plotColors[index % root.plotColors.length]
                        property var series: {
                            if (root.lapDetail) {
                                appController.outingLapDetailState;
                                appController.selectedOutingLap;
                                return appController.outingLapSeries(channelName, Math.max(100, Math.round(width * 1.5)));
                            }
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
                                width: Math.min(220, parent.width * 0.4, Math.max(160, parent.width * 0.24))
                                spacing: 0
                                Row {
                                    width: channelInfo.width
                                    spacing: 2
                                    FeComboBox {
                                        objectName: "analysisReplaceChannel-" + chartRow.channelName
                                        popupMinimumWidth: 320
                                        wrapPopupText: true
                                        width: channelInfo.width - 22
                                        implicitHeight: 26
                                        model: root.availableChannels.filter(channel => channel === chartRow.channelName || root.visibleChannels.indexOf(channel) < 0)
                                        currentIndex: model.indexOf(chartRow.channelName)
                                        ToolTip.visible: hovered
                                        ToolTip.text: chartRow.series.brakingUp === true ? qsTr("Longitudinal G · braking upward") : chartRow.channelName
                                        onActivated: index => root.replaceChannel(chartRow.channelName, model[index])
                                    }
                                    ToolButton {
                                        objectName: "analysisRemoveChannel-" + chartRow.channelName
                                        width: 20
                                        height: 26
                                        text: "×"
                                        Accessible.name: qsTr("Remove %1").arg(chartRow.channelName)
                                        onClicked: root.toggleChannel(chartRow.channelName)
                                    }
                                }
                                Row {
                                    spacing: 5
                                    Label {
                                        text: {
                                            if (root.lapDetail) {
                                                appController.outingLapCursor;
                                                appController.outingLapDetailState;
                                                return appController.outingLapValueText(chartRow.channelName);
                                            }
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
                                    onAvailableChanged: if (available) requestPaint()
                                    onVisibleChanged: if (visible) requestPaint()
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
                                                const y = root.graphY(Number(points[pointIndex].y), low, high, height, plotSeries.brakingUp === true);
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
                                                            root.graphY(Number(points[0].y), low, high, height, plotSeries.brakingUp === true),
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
                                    x: Math.max(0, Math.min(parent.width - width, (root.cursorTime - root.rangeStart) / Math.max(0.001, root.rangeEnd - root.rangeStart) * parent.width))
                                    width: 1
                                    height: parent.height
                                    color: "#f3f6fa"
                                    opacity: 0.8
                                }
                                MouseArea {
                                    objectName: "analysisPlotPointer"
                                    anchors.fill: parent
                                    hoverEnabled: root.lapDetail
                                    cursorShape: Qt.CrossCursor
                                    onPressed: mouse => root.seekAt(mouse.x / width)
                                    onPositionChanged: mouse => {
                                        if (pressed || root.lapDetail)
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
                visible: root.visibleChannels.length === 0
                text: root.lapDetail ? qsTr("No speed or G channels recorded in this source") : appController.channelNames.length ? qsTr("Add a telemetry channel to begin analysis") : qsTr("Open a VBO file to inspect telemetry")
                color: "#657386"
                font.pixelSize: 11
            }
        }
    }
}
