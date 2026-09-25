pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// KAN-57: the group's actual best lap against its sector theoretical best,
// sector by sector, with the donor lap each fastest sector came from.
// Selecting a sector opens the donor lap against the actual best in the
// Corner Analyzer.
Dialog {
    id: root
    objectName: "theoreticalBestDialog"
    title: qsTr("Sector theoretical best")
    modal: true
    anchors.centerIn: parent
    width: Math.min(820, parent.width - 40)
    height: Math.min(640, parent.height - 30)
    standardButtons: Dialog.Close
    readonly property var theoretical: appController.outingTheoreticalBest
    readonly property var actualBest: root.theoretical.actualBest || null
    onOpened: if (["idle", "error"].indexOf(root.theoretical.state) >= 0) appController.requestOutingTheoreticalBest()

    function duration(seconds) {
        if (seconds === null || seconds === undefined || !isFinite(seconds)) return "—";
        const ms = Math.round(seconds * 1000);
        return Math.floor(ms / 60000) + ":" + (Math.floor(ms / 1000) % 60).toString().padStart(2, "0")
            + "." + (ms % 1000).toString().padStart(3, "0");
    }
    function seconds(value) {
        return value === null || value === undefined ? "—" : Number(value).toFixed(3) + " s";
    }
    function signedSeconds(value) {
        if (value === null || value === undefined) return "—";
        return (value >= 0 ? "+" : "") + Number(value).toFixed(3) + " s";
    }

    contentItem: Item {
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            Label {
                Layout.fillWidth: true
                text: appController.outingRanking.groupLabel || qsTr("Choose a compatibility group in Day results")
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
                color: "#dce4ee"
            }
            RowLayout {
                Layout.fillWidth: true
                visible: root.theoretical.state === "loading"
                BusyIndicator { running: parent.visible; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
                Label { text: qsTr("Timing every eligible lap on one shared track axis…"); color: "#91a0b2" }
            }
            Label {
                objectName: "theoreticalBestMessage"
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.theoretical.message || ""
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
                color: "#d6a457"
            }
            FeButton {
                objectName: "retryTheoreticalBest"
                visible: ["unavailable", "error"].indexOf(root.theoretical.state) >= 0
                text: qsTr("Calculate again")
                onClicked: appController.requestOutingTheoreticalBest()
            }
            GridLayout {
                objectName: "theoreticalBestSummary"
                Layout.fillWidth: true
                visible: root.theoretical.state === "ready"
                columns: 2
                columnSpacing: 16
                rowSpacing: 4
                Label { text: qsTr("Actual best"); color: "#91a0b2" }
                Label {
                    objectName: "theoreticalBestActual"
                    Layout.fillWidth: true
                    text: root.actualBest
                        ? root.duration(root.actualBest.coversWholeLap ? root.actualBest.lapSeconds : root.actualBest.sectorSumSeconds)
                            + " · " + root.actualBest.label
                        : qsTr("Unavailable")
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: "#f2f6fb"
                }
                Label { text: qsTr("Sector theoretical"); color: "#91a0b2" }
                Label {
                    objectName: "theoreticalBestTotal"
                    text: root.duration(root.theoretical.totalSeconds)
                    color: "#55e6a5"
                    font.bold: true
                }
                Label { text: qsTr("Difference"); color: "#91a0b2" }
                Label {
                    objectName: "theoreticalBestDifference"
                    text: root.signedSeconds(root.theoretical.differenceSeconds)
                    color: "#f2f6fb"
                }
            }
            Label {
                objectName: "theoreticalBestExplanation"
                Layout.fillWidth: true
                visible: root.theoretical.state === "ready"
                text: qsTr("Algorithm: %1. The sum of the fastest recorded time for each approved sector across the eligible laps of this group, every lap timed on one shared track axis. It combines fragments of different laps and does not show that the whole lap can be driven that fast.")
                    .arg(root.theoretical.algorithm || "")
                    + (root.actualBest && !root.actualBest.coversWholeLap
                        ? " " + qsTr("The approved sectors do not cover the whole lap, so the actual best above is the sum of its own times over the same sectors, not its lap time.")
                        : "")
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
                font.pixelSize: 11
                color: "#91a0b2"
            }
            ListView {
                id: sectors
                objectName: "theoreticalBestSectors"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 80
                visible: root.theoretical.state === "ready"
                clip: true
                spacing: 4
                model: root.theoretical.sectors || []
                ScrollBar.vertical: ScrollBar {}
                header: RowLayout {
                    width: sectors.width - 18
                    spacing: 8
                    Label { text: qsTr("Sector"); color: "#657386"; font.pixelSize: 11; Layout.fillWidth: true }
                    Label { text: qsTr("Best"); color: "#657386"; font.pixelSize: 11; Layout.preferredWidth: 80 }
                    Label { text: qsTr("Donor lap"); color: "#657386"; font.pixelSize: 11; Layout.preferredWidth: 170 }
                    Label { text: qsTr("Actual best"); color: "#657386"; font.pixelSize: 11; Layout.preferredWidth: 80 }
                    Label { text: qsTr("Loss"); color: "#657386"; font.pixelSize: 11; Layout.preferredWidth: 80 }
                }
                delegate: ItemDelegate {
                    id: sectorRow
                    required property var modelData
                    required property int index
                    objectName: "theoreticalBestSector" + index
                    width: sectors.width - 18
                    enabled: sectorRow.modelData.seconds !== undefined
                    onClicked: {
                        if (appController.openTheoreticalBestSector(sectorRow.modelData.segmentId)) root.close();
                    }
                    ToolTip.visible: hovered && enabled
                    ToolTip.text: qsTr("Open the donor lap against the actual best in the Corner Analyzer")
                    contentItem: RowLayout {
                        spacing: 8
                        Label {
                            Layout.fillWidth: true
                            text: sectorRow.modelData.name + " · " + sectorRow.modelData.type
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: "#dce4ee"
                        }
                        Label {
                            Layout.preferredWidth: 80
                            text: root.seconds(sectorRow.modelData.seconds)
                            color: "#55e6a5"
                        }
                        Label {
                            Layout.preferredWidth: 170
                            text: sectorRow.modelData.seconds !== undefined
                                ? (sectorRow.modelData.sourceLapLabel || qsTr("Lap unavailable"))
                                : (sectorRow.modelData.unavailableReason || "")
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: sectorRow.modelData.seconds !== undefined ? "#dce4ee" : "#d6a457"
                        }
                        Label {
                            Layout.preferredWidth: 80
                            text: root.seconds(sectorRow.modelData.actualSeconds)
                            color: "#b5c1d0"
                        }
                        Label {
                            objectName: "theoreticalBestLoss" + sectorRow.index
                            Layout.preferredWidth: 80
                            text: root.signedSeconds(sectorRow.modelData.lossSeconds)
                            color: "#f2f6fb"
                        }
                    }
                }
            }
        }
    }
}
