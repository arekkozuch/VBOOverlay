pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

Window {
    id: root
    required property url videoSource
    required property real playbackPosition
    required property bool playbackRunning
    required property real mediaDuration
    signal seekRequested(real milliseconds)
    signal togglePlaybackRequested

    visible: appController.analysisVisible
    width: appController.analysisWindowWidth
    height: appController.analysisWindowHeight
    minimumWidth: 760
    minimumHeight: 480
    x: appController.analysisWindowX >= 0 ? appController.analysisWindowX : Screen.width / 2 - width / 2
    y: appController.analysisWindowY >= 0 ? appController.analysisWindowY : Screen.height / 2 - height / 2
    title: qsTr("FlappedEar · Telemetry Analysis")
    color: "#070b10"
    flags: Qt.Window
    transientParent: null
    property bool wasShown: false

    function saveLayout() {
        appController.saveAnalysisWindowState(x, y, width, height, Math.max(220, contextColumn.width), Math.max(130, videoPane.height));
    }

    onClosing: close => {
        saveLayout();
        appController.analysisVisible = false;
    }
    onVisibleChanged: {
        if (visible)
            wasShown = true;
        else if (wasShown)
            saveLayout();
    }
    onPlaybackPositionChanged: {
        if (Math.abs(analysisPlayer.position - playbackPosition) > 180)
            analysisPlayer.position = playbackPosition;
    }
    onPlaybackRunningChanged: {
        if (playbackRunning)
            analysisPlayer.play();
        else
            analysisPlayer.pause();
    }

    MediaPlayer {
        id: analysisPlayer
        source: root.videoSource
        videoOutput: analysisVideo
        audioOutput: AudioOutput {
            muted: true
        }
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia) {
                position = root.playbackPosition;
                if (root.playbackRunning)
                    play();
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: "#0c1118"
            border.color: "#202a36"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10
                Label {
                    text: qsTr("TELEMETRY ANALYSIS")
                    color: "#dce5ee"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.1
                }
                Label {
                    text: qsTr("Drag the separators to resize video, map and charts")
                    color: "#657386"
                    font.pixelSize: 10
                }
                Item {
                    Layout.fillWidth: true
                }
                Label {
                    text: Math.floor(root.playbackPosition / 60000).toString().padStart(2, "0") + ":" + Math.floor((root.playbackPosition / 1000) % 60).toString().padStart(2, "0")
                    color: "#9ca9b8"
                    font.family: "Menlo"
                    font.pixelSize: 10
                }
                FeButton {
                    compact: true
                    accent: root.playbackRunning
                    text: root.playbackRunning ? "Ⅱ" : "▶"
                    onClicked: root.togglePlaybackRequested()
                }
            }
        }

        SplitView {
            id: workspaceSplit
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal
            handle: Rectangle {
                implicitWidth: 7
                color: SplitHandle.pressed ? "#55e6a5" : SplitHandle.hovered ? "#334556" : "#18232e"
            }

            SplitView {
                id: contextColumn
                SplitView.preferredWidth: Math.max(220, appController.analysisSidebarWidth)
                SplitView.minimumWidth: 220
                orientation: Qt.Vertical
                handle: Rectangle {
                    implicitHeight: 7
                    color: SplitHandle.pressed ? "#55e6a5" : SplitHandle.hovered ? "#334556" : "#18232e"
                }

                Rectangle {
                    id: videoPane
                    SplitView.preferredHeight: Math.max(130, appController.analysisVideoHeight)
                    SplitView.minimumHeight: 130
                    color: "#020304"
                    border.color: "#1c2631"
                    VideoOutput {
                        id: analysisVideo
                        anchors.fill: parent
                        fillMode: VideoOutput.PreserveAspectFit
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: !root.videoSource.toString()
                        text: qsTr("No video loaded")
                        color: "#657386"
                    }
                }

                TrackMapPanel {
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 130
                }
            }

            AnalysisPanel {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 420
                mediaDuration: root.mediaDuration
                onSeekRequested: milliseconds => root.seekRequested(milliseconds)
            }
        }
    }
}
