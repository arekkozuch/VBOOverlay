pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// KAN-60: the day's largest observed losses. Each eligible lap of the
// comparison group is compared with the group's best lap, one window per
// approved segment.
Dialog {
    id: root
    objectName: "timeLossDialog"
    title: qsTr("Largest time losses")
    modal: true
    anchors.centerIn: parent
    width: Math.min(900, parent.width - 40)
    height: Math.min(640, parent.height - 30)
    standardButtons: Dialog.Close
    readonly property var ranking: appController.outingTimeLossRanking
    signal lossSelected(var loss)
    // KAN-61: the loss last opened, so returning from its evidence shows the
    // same row again (matched by lap and segment, since a recalculation can
    // reorder the list).
    property var returnKey: null
    function lossKey(loss) { return JSON.stringify(loss.lapReference) + "|" + loss.segmentId; }
    function restoreSelection() {
        if (!root.returnKey) return;
        const list = root.ranking.losses || [];
        const index = list.findIndex(loss => root.lossKey(loss) === root.returnKey);
        losses.currentIndex = index;
        if (index >= 0) losses.positionViewAtIndex(index, ListView.Center);
    }
    function calculateIfNeeded() {
        if (root.visible && ["idle", "error"].indexOf(root.ranking.state) >= 0) appController.requestOutingTheoreticalBest();
    }
    onOpened: {
        root.calculateIfNeeded();
        Qt.callLater(root.restoreSelection);
    }
    // Exclusions, a new reference or edited segments invalidate the result;
    // recalculate while the dialog is showing.
    onRankingChanged: {
        if (root.ranking.state === "idle") Qt.callLater(root.calculateIfNeeded);
        else if (root.ranking.state === "ready") Qt.callLater(root.restoreSelection);
    }

    function roleText(loss) {
        if (loss.role === "continuation")
            return qsTr("after %1").arg(loss.cornerName || qsTr("corner"));
        return loss.role;
    }
    function percent(value) { return Math.round(Number(value || 0) * 100) + "%"; }

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
                visible: root.ranking.state === "loading"
                BusyIndicator { running: parent.visible; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
                Label { text: qsTr("Timing every eligible lap on one shared track axis…"); color: "#91a0b2" }
            }
            Label {
                objectName: "timeLossMessage"
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.ranking.message || ""
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
                color: "#d6a457"
            }
            FeButton {
                visible: ["unavailable", "error"].indexOf(root.ranking.state) >= 0
                text: qsTr("Calculate again")
                onClicked: appController.requestOutingTheoreticalBest()
            }
            FeCheckBox {
                objectName: "timeLossAllLaps"
                visible: root.ranking.state === "ready"
                text: qsTr("Include every eligible lap (otherwise each run's best lap)")
                checked: appController.outingTimeLossAllLaps
                onToggled: appController.outingTimeLossAllLaps = checked
            }
            Label {
                objectName: "timeLossSummary"
                Layout.fillWidth: true
                visible: root.ranking.state === "ready"
                text: qsTr("Reference: %1 · %2 %3 compared · %4 losses observed%5")
                    .arg(root.ranking.referenceLabel || "—").arg(root.ranking.comparedLapCount || 0)
                    .arg(root.ranking.scope === "allLaps" ? qsTr("laps") : qsTr("run-best laps"))
                    .arg(root.ranking.observationCount || 0)
                    .arg(root.ranking.untimedWindowCount > 0
                        ? qsTr(" · %1 windows without full coverage left out").arg(root.ranking.untimedWindowCount) : "")
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
                color: "#f2f6fb"
            }
            Label {
                objectName: "timeLossExplanation"
                Layout.fillWidth: true
                visible: root.ranking.state === "ready"
                text: qsTr("Method: %1. Each loss is the extra time a lap took through one approved segment compared with the reference, both timed on one shared track axis. A straight right after a corner is its own window, so time lost on the exit is not counted in the corner. An observed loss is not a guaranteed or necessarily safe gain.")
                    .arg(root.ranking.algorithm || "")
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
                font.pixelSize: 11
                color: "#91a0b2"
            }
            ListView {
                id: losses
                objectName: "timeLossList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 80
                visible: root.ranking.state === "ready"
                clip: true
                spacing: 4
                model: root.ranking.losses || []
                ScrollBar.vertical: ScrollBar {}
                header: RowLayout {
                    width: losses.width - 18
                    spacing: 8
                    Label { text: "#"; color: "#657386"; font.pixelSize: 11; Layout.preferredWidth: 28 }
                    Label { text: qsTr("Loss"); color: "#657386"; font.pixelSize: 11; Layout.preferredWidth: 80 }
                    Label { text: qsTr("Segment"); color: "#657386"; font.pixelSize: 11; Layout.fillWidth: true }
                    Label { text: qsTr("Lap"); color: "#657386"; font.pixelSize: 11; Layout.preferredWidth: 180 }
                    Label { text: qsTr("Coverage"); color: "#657386"; font.pixelSize: 11; Layout.preferredWidth: 120 }
                }
                Label {
                    anchors.centerIn: parent
                    visible: losses.count === 0 && root.ranking.state === "ready"
                    text: qsTr("No lap lost time to the reference in any timed segment.")
                    color: "#91a0b2"
                }
                delegate: ItemDelegate {
                    id: lossRow
                    required property var modelData
                    required property int index
                    objectName: "timeLoss" + index
                    width: losses.width - 18
                    highlighted: ListView.isCurrentItem
                    onClicked: root.lossSelected(lossRow.modelData)
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Open this lap against the reference in the Corner Analyzer, zoomed to this segment")
                    contentItem: RowLayout {
                        spacing: 8
                        Label { text: lossRow.index + 1; color: "#657386"; Layout.preferredWidth: 28 }
                        Label {
                            text: "+" + Number(lossRow.modelData.lossSeconds).toFixed(3) + " s"
                            color: "#d95926"
                            font.bold: true
                            Layout.preferredWidth: 80
                        }
                        Label {
                            Layout.fillWidth: true
                            text: lossRow.modelData.name + " · " + root.roleText(lossRow.modelData)
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: "#dce4ee"
                        }
                        Label {
                            Layout.preferredWidth: 180
                            text: lossRow.modelData.lapLabel || qsTr("Lap unavailable")
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: "#dce4ee"
                        }
                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("lap %1 · ref %2").arg(root.percent(lossRow.modelData.coverageLap))
                                .arg(root.percent(lossRow.modelData.coverageReference))
                            color: "#91a0b2"
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }
    }
}
