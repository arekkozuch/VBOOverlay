pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// KAN-48: review automatic straight/corner proposals for the open lap.
// Proposals are review input only. Approving one writes an ordinary segment
// into the run's approved set; that set's revision is what sector, theoretical
// lap and report results record, so the header always names it.
Rectangle {
    id: root
    color: "#070b10"
    border.color: "#1c2631"
    radius: 8
    property int selectedIndex: -1
    property string actionError: ""
    readonly property string reviewState: appController.segmentReviewState
    readonly property var approved: appController.segmentReviewApproved
    readonly property var items: appController.segmentReviewItems
    readonly property var types: ["corner", "straight", "sector"]

    function meters(value) { return Number(value).toFixed(1) + " m"; }
    function stateText(state) {
        return state === "approved" ? qsTr("Approved")
            : state === "rejected" ? qsTr("Rejected")
            : state === "superseded" ? qsTr("Overlaps approved")
            : qsTr("Proposed");
    }
    function stateColor(state) {
        return state === "approved" ? "#55e6a5"
            : state === "rejected" ? "#657386"
            : state === "superseded" ? "#ff8f99"
            : "#4da3ff";
    }
    function typeText(type) {
        return type === "corner" ? qsTr("Corner") : type === "straight" ? qsTr("Straight") : qsTr("Sector");
    }

    Component.onCompleted: if (root.reviewState === "idle") appController.requestSegmentReview()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("TRACK SEGMENTS")
                color: "#687789"
                font.pixelSize: 9
                font.letterSpacing: 1
                Layout.fillWidth: true
            }
            FeButton {
                objectName: "recomputeSegmentProposals"
                compact: true
                text: qsTr("Recompute proposals")
                enabled: root.reviewState !== "loading"
                onClicked: { root.actionError = ""; root.selectedIndex = -1; appController.requestSegmentReview(); }
            }
        }

        // The approved revision is shown apart from the proposals below.
        Rectangle {
            objectName: "approvedSegmentRevision"
            Layout.fillWidth: true
            implicitHeight: approvedColumn.implicitHeight + 16
            radius: 6
            color: "#0d151d"
            border.color: Number(root.approved.count || 0) > 0 ? "#2b6b52" : "#22303d"
            visible: root.reviewState !== "idle"
            ColumnLayout {
                id: approvedColumn
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Number(root.approved.count || 0) > 0 ? "#55e6a5" : "#91a0b2"
                    font.weight: Font.DemiBold
                    text: Number(root.approved.count || 0) > 0
                        ? qsTr("Approved revision %1 · %2 segment(s)").arg(root.approved.shortRevision).arg(root.approved.count)
                        : qsTr("No approved segments for this track configuration")
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: "#657386"
                    font.pixelSize: 10
                    text: qsTr("Only approved segments are used by sector, theoretical-lap and report results, which record this revision. Proposals and rejections are not saved.")
                }
                Repeater {
                    model: (root.approved.segments || []).filter(segment => !segment.matchesProposal)
                    delegate: RowLayout {
                        id: otherSegment
                        required property var modelData
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            color: "#b5c0cd"
                            font.pixelSize: 11
                            text: otherSegment.modelData.name + " · " + root.typeText(otherSegment.modelData.type) + " · "
                                + root.meters(otherSegment.modelData.startMeters) + " – " + root.meters(otherSegment.modelData.endMeters)
                        }
                        FeButton {
                            compact: true
                            danger: true
                            text: qsTr("Revoke")
                            onClicked: appController.revokeApprovedSegment(otherSegment.modelData.id)
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: Number(root.approved.otherConfigurationCount || 0) > 0
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "#ffb84d"
                        font.pixelSize: 11
                        text: qsTr("%1 segment(s) were approved for a different track configuration and are not applied.")
                            .arg(root.approved.otherConfigurationCount)
                    }
                    FeButton {
                        objectName: "discardOtherConfigurationSegments"
                        compact: true
                        danger: true
                        text: qsTr("Discard them")
                        onClicked: appController.discardOtherConfigurationSegments()
                    }
                }
            }
        }

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: root.reviewState === "loading"
            visible: running
        }
        Label {
            objectName: "segmentReviewMessage"
            Layout.fillWidth: true
            visible: text.length > 0
            wrapMode: Text.WordWrap
            color: root.reviewState === "error" ? "#ff8f99" : "#ffb84d"
            text: root.actionError || appController.segmentReviewMessage
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.reviewState === "ready"
            Label {
                Layout.fillWidth: true
                color: "#91a0b2"
                font.pixelSize: 11
                text: qsTr("%1 proposal(s) · axis %2").arg(root.items.length).arg(root.meters(appController.segmentReviewAxisLength))
            }
            FeButton {
                objectName: "approveCertainSegments"
                compact: true
                accent: true
                text: qsTr("Approve all certain")
                enabled: root.items.some(item => item.state === "proposed" && item.certain)
                onClicked: {
                    root.actionError = appController.approveCertainSegmentProposals() > 0
                        ? "" : qsTr("No certain proposal could be approved.");
                }
            }
        }

        ListView {
            id: list
            objectName: "segmentProposalList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.reviewState === "ready"
            clip: true
            spacing: 6
            model: root.items
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                id: row
                required property var modelData
                required property int index
                property bool editing: false
                width: ListView.view.width - 10
                implicitHeight: rowColumn.implicitHeight + 14
                radius: 6
                color: root.selectedIndex === index ? "#132030" : "#0b1118"
                border.color: root.selectedIndex === index ? "#4da3ff" : "#1c2631"
                opacity: modelData.state === "rejected" ? 0.6 : 1.0
                TapHandler { onTapped: root.selectedIndex = row.index }

                ColumnLayout {
                    id: rowColumn
                    anchors.fill: parent
                    anchors.margins: 7
                    spacing: 4
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Rectangle {
                            radius: 4
                            color: "transparent"
                            border.color: root.stateColor(row.modelData.state)
                            implicitWidth: stateLabel.implicitWidth + 10
                            implicitHeight: stateLabel.implicitHeight + 4
                            Label {
                                id: stateLabel
                                anchors.centerIn: parent
                                text: root.stateText(row.modelData.state)
                                color: root.stateColor(row.modelData.state)
                                font.pixelSize: 10
                            }
                        }
                        Label {
                            text: row.modelData.name + (row.modelData.edited ? qsTr(" (edited)") : "")
                            color: "#f2f6fb"
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: root.typeText(row.modelData.type)
                                + (row.modelData.turnDegrees !== undefined
                                    ? " · " + Math.abs(Math.round(row.modelData.turnDegrees)) + "°"
                                        + (row.modelData.turnDegrees > 0 ? qsTr(" left") : qsTr(" right")) : "")
                            color: "#91a0b2"
                            font.pixelSize: 11
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        color: "#b5c0cd"
                        font.family: "Menlo"
                        font.pixelSize: 11
                        text: root.meters(row.modelData.startMeters) + " ±" + Number(row.modelData.startToleranceMeters).toFixed(0)
                            + " → " + root.meters(row.modelData.endMeters) + " ±" + Number(row.modelData.endToleranceMeters).toFixed(0)
                            + "  (" + root.meters(row.modelData.lengthMeters) + ")"
                            + (row.modelData.wrapsGate ? qsTr("  crosses start/finish") : "")
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        wrapMode: Text.WordWrap
                        color: "#ffb84d"
                        font.pixelSize: 11
                        text: {
                            const parts = [];
                            if (row.modelData.startUncertainty.length > 0)
                                parts.push(qsTr("Start uncertain: ") + row.modelData.startUncertainty.join(", "));
                            if (row.modelData.endUncertainty.length > 0)
                                parts.push(qsTr("End uncertain: ") + row.modelData.endUncertainty.join(", "));
                            return parts.join(" · ");
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: row.modelData.apexMeters !== undefined || row.modelData.apexNote !== undefined
                        color: "#91a0b2"
                        font.pixelSize: 11
                        text: row.modelData.apexMeters !== undefined
                            ? qsTr("Geometric apex %1 ±%2 m").arg(root.meters(row.modelData.apexMeters))
                                .arg(Number(row.modelData.apexToleranceMeters).toFixed(0))
                            : (row.modelData.apexNote || "")
                    }
                    RowLayout {
                        visible: !row.editing
                        spacing: 6
                        FeButton {
                            objectName: "approveSegmentProposal"
                            compact: true
                            accent: true
                            text: qsTr("Approve")
                            visible: row.modelData.state === "proposed" || row.modelData.state === "rejected"
                            onClicked: root.actionError = appController.approveSegmentProposal(row.index)
                        }
                        FeButton {
                            objectName: "rejectSegmentProposal"
                            compact: true
                            text: row.modelData.state === "rejected" ? qsTr("Restore") : qsTr("Reject")
                            visible: row.modelData.state === "proposed" || row.modelData.state === "rejected"
                            onClicked: {
                                root.actionError = "";
                                appController.setSegmentProposalRejected(row.index, row.modelData.state !== "rejected");
                            }
                        }
                        FeButton {
                            objectName: "editSegmentProposal"
                            compact: true
                            text: qsTr("Edit")
                            visible: row.modelData.state !== "approved"
                            onClicked: {
                                nameField.text = row.modelData.name;
                                typeBox.currentIndex = root.types.indexOf(row.modelData.type);
                                startField.text = Number(row.modelData.startMeters).toFixed(1);
                                endField.text = Number(row.modelData.endMeters).toFixed(1);
                                row.editing = true;
                                root.selectedIndex = row.index;
                            }
                        }
                        FeButton {
                            objectName: "revokeSegmentApproval"
                            compact: true
                            danger: true
                            text: qsTr("Revoke approval")
                            visible: row.modelData.state === "approved"
                            onClicked: appController.revokeApprovedSegment(row.modelData.approvedSegmentId)
                        }
                    }
                    GridLayout {
                        visible: row.editing
                        Layout.fillWidth: true
                        columns: 4
                        columnSpacing: 6
                        rowSpacing: 6
                        FeTextField {
                            id: nameField
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            maximumLength: 160
                            placeholderText: qsTr("Name")
                            Accessible.name: qsTr("Segment name")
                        }
                        FeComboBox {
                            id: typeBox
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            model: root.types.map(type => root.typeText(type))
                            Accessible.name: qsTr("Segment type")
                        }
                        FeTextField {
                            id: startField
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            placeholderText: qsTr("Start (m)")
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            Accessible.name: qsTr("Start progress in meters")
                        }
                        FeTextField {
                            id: endField
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            placeholderText: qsTr("End (m)")
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            Accessible.name: qsTr("End progress in meters")
                        }
                        FeButton {
                            compact: true
                            accent: true
                            text: qsTr("Save edit")
                            onClicked: {
                                // An untouched field keeps the exact proposed boundary, not its rounded display.
                                const original = row.modelData;
                                const parse = text => text.trim().length > 0 ? Number(text) : NaN;
                                const start = startField.text === Number(original.startMeters).toFixed(1)
                                    ? original.startMeters : parse(startField.text);
                                const end = endField.text === Number(original.endMeters).toFixed(1)
                                    ? original.endMeters : parse(endField.text);
                                const error = appController.editSegmentProposal(row.index, nameField.text,
                                    root.types[typeBox.currentIndex], start, end);
                                root.actionError = error;
                                if (error.length === 0) row.editing = false;
                            }
                        }
                        FeButton {
                            compact: true
                            text: qsTr("Cancel")
                            onClicked: { row.editing = false; root.actionError = ""; }
                        }
                    }
                }
            }
        }
        Item { visible: root.reviewState !== "ready"; Layout.fillHeight: true }
    }
}
