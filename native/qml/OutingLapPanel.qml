pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#090e14"
    function duration(seconds) {
        const minutes = Math.floor(seconds / 60);
        return minutes + ":" + (seconds - minutes * 60).toFixed(3).padStart(6, "0");
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("All laps"); color: "#f2f6fb"; font.pixelSize: 24; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            Label {
                text: qsTr("%1 runs · %2 recorded sections").arg(appController.eventRuns.length).arg(appController.outingLaps.length)
                color: "#91a0b2"
                font.pixelSize: 12
            }
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Click a row to open it. Chronological order · UTC. OUT before the first start/finish crossing; IN after the last. Entries without a reliable clock or crossing are marked.")
            wrapMode: Text.WordWrap
            color: "#657386"
            font.pixelSize: 11
        }
        BusyIndicator { running: appController.outingLapsLoading; visible: running; Layout.alignment: Qt.AlignHCenter }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(80, notices.implicitHeight)
            visible: appController.outingLapMessages.length > 0
            contentWidth: availableWidth
            Label {
                id: notices
                width: parent.width
                text: appController.outingLapMessages.join("\n")
                color: "#d6a457"
                wrapMode: Text.WordWrap
                font.pixelSize: 11
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: "#101720"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12; anchors.rightMargin: 12
                spacing: 12
                Label { text: "#"; Layout.preferredWidth: 32; color: "#718092" }
                Label { text: qsTr("Start · UTC"); Layout.preferredWidth: 176; color: "#718092" }
                Label { text: qsTr("Type"); Layout.preferredWidth: 76; color: "#718092" }
                Label { text: qsTr("Run"); Layout.fillWidth: true; color: "#718092" }
                Label { text: qsTr("Duration"); Layout.preferredWidth: 92; horizontalAlignment: Text.AlignRight; color: "#718092" }
            }
        }
        ListView {
            id: laps
            objectName: "outingLapList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: appController.outingLaps
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: Rectangle {
                id: row
                required property var modelData
                required property int index
                width: laps.width
                height: 48
                radius: 6
                objectName: "outingLapRow" + index
                activeFocusOnTab: true
                color: pointer.containsMouse || activeFocus ? "#193529" : index % 2 ? "#0c131b" : "#101923"
                border.color: activeFocus ? "#55e6a5" : "transparent"
                Accessible.role: Accessible.Button
                Accessible.name: modelData.type + " " + modelData.lapNumber + " · " + modelData.runName
                Accessible.onPressAction: appController.selectOutingLap(index)
                Keys.onReturnPressed: appController.selectOutingLap(index)
                Keys.onEnterPressed: appController.selectOutingLap(index)
                Keys.onSpacePressed: appController.selectOutingLap(index)
                MouseArea {
                    id: pointer
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { row.forceActiveFocus(); appController.selectOutingLap(row.index); }
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12; anchors.rightMargin: 12
                    spacing: 12
                    Label { text: row.index + 1; Layout.preferredWidth: 32; color: "#657386"; font.pixelSize: 11 }
                    Label { text: row.modelData.clock; Layout.preferredWidth: 176; color: "#aab6c4"; font.pixelSize: 11; elide: Text.ElideRight }
                    Label {
                        text: row.modelData.type === "LAP" ? "LAP " + row.modelData.lapNumber : row.modelData.type
                        Layout.preferredWidth: 76
                        color: row.modelData.type === "LAP" ? "#55e6a5" : row.modelData.type === "UNKNOWN" ? "#d6a457" : "#74a9d8"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: row.modelData.runName + (row.modelData.referenceIssue ? " · " + row.modelData.referenceIssue
                            : row.modelData.bestOfRun ? qsTr(" · Best of run") : "")
                        Layout.fillWidth: true
                        color: row.modelData.referenceIssue ? "#d6a457" : "#dce4ee"
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                    }
                    Label { text: root.duration(row.modelData.durationSeconds); Layout.preferredWidth: 92; horizontalAlignment: Text.AlignRight; color: "#f2f6fb"; font.family: "Menlo"; font.pixelSize: 12 }
                }
            }
            Label {
                anchors.centerIn: parent
                visible: laps.count === 0 && !appController.outingLapsLoading
                text: qsTr("No recorded lap sections available")
                color: "#657386"
            }
        }
    }
}
