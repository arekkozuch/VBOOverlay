pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// KAN-64: each approved section's typical time (median) and spread
// (interquartile range) per session, in chronological order. Green marks the
// session with the quickest typical time in that section; the brighter the
// orange, the slower. Clicking a cell lists the laps behind it.
Item {
    id: root
    signal lapChosen(var reference)
    readonly property var progression: appController.outingSectorProgression
    readonly property var sessions: root.progression.sessions || []
    readonly property var segments: root.progression.segments || []
    readonly property int nameWidth: 150
    readonly property int cellWidth: 138
    property var openCell: null

    function calculateIfNeeded() {
        if (root.visible && ["idle", "error"].indexOf(root.progression.state) >= 0) appController.requestOutingTheoreticalBest();
    }
    onVisibleChanged: root.calculateIfNeeded()
    onProgressionChanged: if (root.progression.state === "idle") Qt.callLater(root.calculateIfNeeded)

    function slowest(row) {
        let worst = 0;
        for (const cell of row.cells)
            if (cell.summary.available) worst = Math.max(worst, cell.summary.median - row.fastestTypical);
        return worst;
    }
    function cellColor(row, cell) {
        if (!cell.summary.available) return "#101923";
        const delta = cell.summary.median - row.fastestTypical;
        if (delta < 0.0005) return "#1d4a37";
        const t = Math.min(1, delta / Math.max(0.001, root.slowest(row)));
        return Qt.rgba(0.20 + 0.45 * t, 0.16 + 0.14 * t, 0.12, 1);
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            visible: root.progression.state === "loading"
            BusyIndicator { running: parent.visible; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
            Label { text: qsTr("Timing every eligible lap by section…"); color: "#91a0b2" }
        }
        Label {
            objectName: "sectionProgressionMessage"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.progression.message || ""
            wrapMode: Text.WordWrap
            color: "#d6a457"
        }
        Label {
            Layout.fillWidth: true
            visible: root.progression.state === "ready"
            text: qsTr("Each cell: typical time (median) and spread (middle half of that session's laps). Green is the quickest session in that section; the brighter the orange, the slower. Fewer than %1 laps: no statistics. Click a cell for its laps.")
                .arg(root.progression.minimumSamples || 3)
            wrapMode: Text.WordWrap
            font.pixelSize: 11
            color: "#91a0b2"
        }
        Flickable {
            id: grid
            objectName: "sectionProgressionGrid"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.progression.state === "ready"
            clip: true
            contentWidth: table.implicitWidth
            contentHeight: table.implicitHeight
            ScrollBar.vertical: ScrollBar {}
            ScrollBar.horizontal: ScrollBar {}
            Column {
                id: table
                spacing: 3
                // Sessions with their context.
                Row {
                    spacing: 3
                    Item { width: root.nameWidth; height: 1 }
                    Repeater {
                        model: root.sessions
                        delegate: Rectangle {
                            id: sessionHeader
                            required property var modelData
                            width: root.cellWidth
                            height: 74
                            color: "#111a24"
                            radius: 4
                            Column {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 1
                                Label {
                                    width: parent.width
                                    text: sessionHeader.modelData.runName
                                    textFormat: Text.PlainText
                                    elide: Text.ElideRight
                                    color: "#f2f6fb"; font.pixelSize: 12; font.weight: Font.DemiBold
                                }
                                Label {
                                    width: parent.width
                                    text: sessionHeader.modelData.laps && sessionHeader.modelData.laps.available
                                        ? qsTr("laps %1 · spread %2 s").arg(appController.formatElapsedTime(Number(sessionHeader.modelData.laps.median)))
                                            .arg(Number(sessionHeader.modelData.laps.interquartileRange).toFixed(1))
                                        : qsTr("%1 laps").arg(sessionHeader.modelData.laps ? sessionHeader.modelData.laps.count : 0)
                                    color: "#91a0b2"; font.pixelSize: 10; elide: Text.ElideRight
                                }
                                Label {
                                    width: parent.width
                                    text: sessionHeader.modelData.conditions ? qsTr("Conditions: %1").arg(sessionHeader.modelData.conditions) : qsTr("Conditions: unknown")
                                    textFormat: Text.PlainText
                                    color: "#657386"; font.pixelSize: 10; elide: Text.ElideRight
                                }
                                Label {
                                    width: parent.width
                                    text: sessionHeader.modelData.setupChanges ? qsTr("Setup: %1").arg(sessionHeader.modelData.setupChanges) : qsTr("Setup: unknown")
                                    textFormat: Text.PlainText
                                    color: "#657386"; font.pixelSize: 10; elide: Text.ElideRight
                                }
                            }
                            ToolTip.visible: headerPointer.containsMouse
                            ToolTip.text: [sessionHeader.modelData.clock, sessionHeader.modelData.conditions,
                                sessionHeader.modelData.setupChanges, sessionHeader.modelData.notes].filter(text => !!text).join("\n")
                            MouseArea { id: headerPointer; anchors.fill: parent; hoverEnabled: true }
                        }
                    }
                }
                Repeater {
                    model: root.segments
                    delegate: Row {
                        id: sectionRow
                        required property var modelData
                        required property int index
                        spacing: 3
                        Label {
                            width: root.nameWidth
                            height: 44
                            verticalAlignment: Text.AlignVCenter
                            text: sectionRow.modelData.name
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: "#dce4ee"; font.pixelSize: 12
                        }
                        Repeater {
                            model: sectionRow.modelData.cells
                            delegate: ItemDelegate {
                                id: cell
                                required property var modelData
                                required property int index
                                objectName: "sectionCell-" + sectionRow.index + "-" + cell.index
                                width: root.cellWidth
                                height: 44
                                padding: 0
                                enabled: cell.modelData.laps.length > 0
                                background: Rectangle {
                                    radius: 4
                                    color: root.cellColor(sectionRow.modelData, cell.modelData)
                                    border.color: root.openCell === cell.modelData || cell.visualFocus ? "#ffffff" : "transparent"
                                }
                                contentItem: Column {
                                    Label {
                                        width: root.cellWidth
                                        horizontalAlignment: Text.AlignHCenter
                                        text: cell.modelData.summary.available
                                            ? appController.formatElapsedTime(Number(cell.modelData.summary.median))
                                            : (cell.modelData.summary.count === 1 ? qsTr("1 lap") : cell.modelData.summary.count > 0 ? qsTr("%1 laps").arg(cell.modelData.summary.count) : "—")
                                        color: "#f2f6fb"; font.pixelSize: 12; font.weight: Font.DemiBold
                                    }
                                    Label {
                                        width: root.cellWidth
                                        horizontalAlignment: Text.AlignHCenter
                                        visible: cell.modelData.summary.available
                                        text: qsTr("spread %1 s · %2").arg(Number(cell.modelData.summary.interquartileRange).toFixed(3))
                                            .arg(cell.modelData.summary.count)
                                        color: "#b5c1d0"; font.pixelSize: 10
                                    }
                                }
                                onClicked: {
                                    root.openCell = cell.modelData;
                                    lapList.title = qsTr("%1 · %2").arg(sectionRow.modelData.name)
                                        .arg(root.sessions[cell.index] ? root.sessions[cell.index].runName : "");
                                    lapList.open();
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    // The laps behind one cell, quickest first; choosing one opens it.
    Popup {
        id: lapList
        objectName: "sectionCellLaps"
        property string title: ""
        anchors.centerIn: parent
        width: 360
        height: Math.min(360, root.height - 20)
        modal: true
        onClosed: root.openCell = null
        background: Rectangle { color: "#0b1119"; border.color: "#253244"; radius: 8 }
        contentItem: ColumnLayout {
            spacing: 6
            Label { text: lapList.title; textFormat: Text.PlainText; color: "#f2f6fb"; font.weight: Font.DemiBold }
            ListView {
                id: cellLaps
                objectName: "sectionCellLapList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.openCell ? root.openCell.laps : []
                delegate: ItemDelegate {
                    id: lapRow
                    required property var modelData
                    width: cellLaps.width
                    text: (lapRow.modelData.label || qsTr("Lap unavailable")) + " · "
                        + appController.formatElapsedTime(Number(lapRow.modelData.seconds))
                    onClicked: { lapList.close(); root.lapChosen(lapRow.modelData.reference); }
                }
            }
        }
    }
}
