pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    signal seekRequested(real milliseconds)

    implicitHeight: 158
    radius: 8
    color: "#070b10"
    border.color: "#1c2631"
    clip: true

    function lapTime(seconds) {
        const safeSeconds = Math.max(0, Number(seconds));
        const minutes = Math.floor(safeSeconds / 60);
        const remaining = safeSeconds - minutes * 60;
        return minutes + ":" + remaining.toFixed(3).padStart(6, "0");
    }

    function delta(seconds) {
        const value = Number(seconds);
        if (!Number.isFinite(value) || value <= 0.0005)
            return qsTr("BEST");
        return "+" + value.toFixed(3);
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("LAP TIMING")
                color: "#8d9aaa"
                font.pixelSize: 9
                font.weight: Font.DemiBold
                font.letterSpacing: 1.2
            }
            Label {
                text: appController.lapTimingStatus
                color: appController.lapSummaries.length > 0 ? "#55e6a5" : "#657386"
                font.pixelSize: 9
            }
            Item { Layout.fillWidth: true }
            Label {
                visible: appController.lapSummaries.length > 0
                text: qsTr("Select a lap to seek")
                color: "#536172"
                font.pixelSize: 9
            }
        }

        ListView {
            id: lapList
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: count > 0
            clip: true
            spacing: 2
            model: appController.lapSummaries
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: lapRow
                required property int index
                required property var modelData
                width: lapList.width
                height: 29
                radius: 4
                color: rowMouse.containsMouse ? "#17232d" : index % 2 ? "#0a1017" : "#0c131b"
                property real seekMilliseconds: {
                    appController.syncOffset;
                    appController.timeScale;
                    appController.telemetryDuration;
                    return appController.videoMillisecondsForTelemetryTime(Number(modelData.startTelemetryTime));
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 9
                    anchors.rightMargin: 9
                    spacing: 10
                    Label {
                        Layout.preferredWidth: 48
                        text: qsTr("Lap %1").arg(lapRow.modelData.number)
                        color: "#aab6c4"
                        font.pixelSize: 10
                    }
                    Label {
                        Layout.preferredWidth: 76
                        text: root.lapTime(lapRow.modelData.durationSeconds)
                        color: "#eef3f8"
                        font.family: "Menlo"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: root.delta(lapRow.modelData.deltaToBestSeconds)
                        color: lapRow.modelData.isBest ? "#55e6a5" : "#ffb84d"
                        font.family: "Menlo"
                        font.pixelSize: 10
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: lapRow.seekMilliseconds >= 0 ? "▶" : qsTr("Outside video")
                        color: lapRow.seekMilliseconds >= 0 ? "#6f8295" : "#465463"
                        font.pixelSize: 9
                    }
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    enabled: lapRow.seekMilliseconds >= 0
                    hoverEnabled: true
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: root.seekRequested(lapRow.seekMilliseconds)
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: appController.lapSummaries.length === 0
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: appController.lapTimingStatus
            color: "#657386"
            font.pixelSize: 10
        }
    }
}
