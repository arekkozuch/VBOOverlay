import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMultimedia

ApplicationWindow {
    id: window
    width: appController.windowWidth
    height: appController.windowHeight
    minimumWidth: 1180
    minimumHeight: 720
    visible: true
    title: "FlappedEar Telemetry"
    color: "#070a0f"
    x: appController.windowX >= 0 ? appController.windowX : Screen.width / 2 - width / 2
    y: appController.windowY >= 0 ? appController.windowY : Screen.height / 2 - height / 2
    palette.window: "#0b1017"
    palette.windowText: "#e8edf4"
    palette.text: "#e8edf4"
    palette.buttonText: "#e8edf4"
    palette.highlight: "#55e6a5"
    palette.highlightedText: "#07140f"

    property bool fullScreenPreview: false
    property int editorVisibility: Window.Windowed
    property bool welcomeVisible: !appController.videoName
    property int selectedWidgetIndex: -1
    property var selectedWidgetIndices: []
    property var widgetCatalog: [
        {
            "label": "Speed",
            "type": "speed",
            "icon": "KM"
        },
        {
            "label": "RPM",
            "type": "rpm",
            "icon": "RPM"
        },
        {
            "label": "Heart rate",
            "type": "heartRate",
            "icon": "♥"
        },
        {
            "label": "Pedals",
            "type": "pedals",
            "icon": "▥"
        },
        {
            "label": "G-Force",
            "type": "gForce",
            "icon": "G"
        },
        {
            "label": "Track",
            "type": "track",
            "icon": "⌁"
        },
        {
            "label": "Custom",
            "type": "customValue",
            "icon": "123"
        },
        {
            "label": "Arc gauge",
            "type": "arcGauge",
            "icon": "◒"
        },
        {
            "label": "Dial gauge",
            "type": "dialGauge",
            "icon": "◉"
        },
        {
            "label": "Data strip",
            "type": "telemetryOverlay",
            "icon": "▤"
        },
        {
            "label": "Retro RPM",
            "type": "retroTachometer",
            "icon": "R"
        },
        {
            "label": "Retro gear",
            "type": "retroGear",
            "icon": "G"
        },
        {
            "label": "Retro pedal",
            "type": "retroPedal",
            "icon": "%"
        },
        {
            "label": "Retro speed",
            "type": "retroSpeedArc",
            "icon": "S"
        },
        {
            "label": "Nameplate",
            "type": "retroNameplate",
            "icon": "ID"
        },
        {
            "label": "Logo",
            "type": "brandLogo",
            "icon": "FE"
        }
    ]

    onClosing: appController.saveWindowState(x, y, width, height)
    onVisibilityChanged: {
        const systemFullScreen = window.visibility === Window.FullScreen;
        if (fullScreenPreview !== systemFullScreen)
            fullScreenPreview = systemFullScreen;
    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("File")
            Action {
                text: qsTr("New Project")
                shortcut: StandardKey.New
                onTriggered: {
                    window.clearWidgetSelection();
                    appController.clearProject();
                    window.welcomeVisible = true;
                }
            }
            Action {
                text: qsTr("Welcome")
                onTriggered: window.welcomeVisible = true
            }
            Action {
                text: qsTr("Open Project…")
                shortcut: StandardKey.Open
                onTriggered: projectOpenDialog.open()
            }
            Action {
                text: qsTr("Save Project As…")
                shortcut: StandardKey.SaveAs
                onTriggered: projectSaveDialog.open()
            }
            Action {
                text: qsTr("Export…")
                enabled: appController.videoName.length > 0 && appController.telemetryName.length > 0
                onTriggered: exportDialog.open()
            }
            MenuSeparator {}
            Action {
                text: qsTr("Open Video…")
                shortcut: "Ctrl+Shift+V"
                onTriggered: videoDialog.open()
            }
            Action {
                text: qsTr("Open VBO…")
                shortcut: "Ctrl+Shift+T"
                onTriggered: vboDialog.open()
            }
            MenuSeparator {}
            Action {
                text: qsTr("Quit")
                shortcut: StandardKey.Quit
                onTriggered: Qt.quit()
            }
        }
        Menu {
            title: qsTr("View")
            Action {
                text: window.fullScreenPreview ? qsTr("Exit Full Screen") : qsTr("Enter Full Screen")
                shortcut: StandardKey.FullScreen
                onTriggered: window.toggleFullScreen()
            }
            Action {
                text: qsTr("Telemetry Analysis")
                checkable: true
                checked: appController.analysisVisible
                shortcut: "Ctrl+Shift+A"
                onTriggered: appController.analysisVisible = checked
            }
        }
    }

    function toggleFullScreen() {
        if (fullScreenPreview || visibility === Window.FullScreen)
            exitFullScreen();
        else
            enterFullScreen();
    }
    function enterFullScreen() {
        if (visibility !== Window.FullScreen)
            editorVisibility = visibility === Window.Maximized ? Window.Maximized : Window.Windowed;
        fullScreenPreview = true;
        visibility = Window.FullScreen;
    }
    function exitFullScreen() {
        fullScreenPreview = false;
        visibility = editorVisibility === Window.Maximized ? Window.Maximized : Window.Windowed;
    }
    function formatTime(milliseconds) {
        const seconds = Math.max(0, milliseconds / 1000);
        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        const remainder = Math.floor(seconds % 60);
        const millis = Math.floor(milliseconds % 1000);
        return (hours > 0 ? String(hours).padStart(2, "0") + ":" : "") + String(minutes).padStart(2, "0") + ":" + String(remainder).padStart(2, "0") + "." + String(millis).padStart(3, "0");
    }
    function templateNames() {
        const names = [];
        for (let index = 0; index < appController.widgetModel.templates.length; ++index)
            names.push(appController.widgetModel.templates[index].name);
        return names;
    }
    function templateIndexById(templateId) {
        for (let index = 0; index < appController.widgetModel.templates.length; ++index) {
            if (appController.widgetModel.templates[index].id === templateId)
                return index;
        }
        return 0;
    }
    function selectedTemplate() {
        const templates = appController.widgetModel.templates;
        return templatePicker.currentIndex >= 0 && templatePicker.currentIndex < templates.length ? templates[templatePicker.currentIndex] : null;
    }
    function selectedWidgetCues() {
        appController.widgetModel.revision;
        return selectedWidgetIndex >= 0 ? (appController.widgetModel.widget(selectedWidgetIndex).cues || []) : [];
    }
    function textEditorHasFocus() {
        let item = activeFocusItem;
        while (item) {
            if (item instanceof TextInput || item instanceof TextEdit)
                return true;
            item = item.parent;
        }
        return false;
    }
    function deleteSelectedWidget() {
        if (selectedWidgetIndex < 0 || selectedWidgetIndex >= appController.widgetModel.count || textEditorHasFocus())
            return;
        appController.widgetModel.removeWidgets(selectedWidgetIndices.length ? selectedWidgetIndices : [selectedWidgetIndex]);
        selectedWidgetIndices = [];
        selectedWidgetIndex = -1;
    }
    function isWidgetSelected(index) {
        return selectedWidgetIndices.indexOf(index) >= 0;
    }
    function clearWidgetSelection() {
        selectedWidgetIndices = [];
        selectedWidgetIndex = -1;
    }
    function selectWidget(index, additive) {
        if (index < 0) {
            clearWidgetSelection();
            return;
        }
        if (additive) {
            const selection = selectedWidgetIndices.slice();
            const existing = selection.indexOf(index);
            if (existing >= 0)
                selection.splice(existing, 1);
            else
                selection.push(index);
            selectedWidgetIndices = selection;
            selectedWidgetIndex = selection.length ? selection[selection.length - 1] : -1;
            return;
        }
        selectedWidgetIndices = appController.widgetModel.groupMembers(index);
        selectedWidgetIndex = index;
    }
    function groupSelectedWidgets() {
        if (selectedWidgetIndices.length >= 2)
            appController.widgetModel.groupWidgets(selectedWidgetIndices);
    }
    function ungroupSelectedWidgets() {
        if (selectedWidgetIndex >= 0) {
            appController.widgetModel.ungroupWidget(selectedWidgetIndex);
            selectedWidgetIndices = [selectedWidgetIndex];
        }
    }
    function selectedWidgetIsGrouped() {
        appController.widgetModel.revision;
        return selectedWidgetIndex >= 0 && !!appController.widgetModel.widget(selectedWidgetIndex).groupId;
    }

    Shortcut {
        sequence: "Escape"
        context: Qt.WindowShortcut
        enabled: window.fullScreenPreview || window.visibility === Window.FullScreen
        onActivated: window.exitFullScreen()
    }
    Shortcut {
        sequence: "Delete"
        context: Qt.WindowShortcut
        enabled: !window.welcomeVisible && window.selectedWidgetIndex >= 0
        onActivated: window.deleteSelectedWidget()
    }
    Shortcut {
        sequence: "Ctrl+G"
        context: Qt.WindowShortcut
        enabled: !window.welcomeVisible && window.selectedWidgetIndices.length >= 2 && !window.textEditorHasFocus()
        onActivated: window.groupSelectedWidgets()
    }
    Shortcut {
        sequence: "Ctrl+Shift+G"
        context: Qt.WindowShortcut
        enabled: !window.welcomeVisible && window.selectedWidgetIndex >= 0 && !window.textEditorHasFocus()
        onActivated: window.ungroupSelectedWidgets()
    }
    Shortcut {
        sequence: "Backspace"
        context: Qt.WindowShortcut
        enabled: !window.welcomeVisible && window.selectedWidgetIndex >= 0
        onActivated: window.deleteSelectedWidget()
    }

    FileDialog {
        id: videoDialog
        title: qsTr("Open motorsport video")
        nameFilters: [qsTr("Video files (*.mp4 *.mov)")]
        onAccepted: appController.loadVideo(selectedFile)
    }
    FileDialog {
        id: vboDialog
        title: qsTr("Open VBOX telemetry")
        nameFilters: [qsTr("VBOX telemetry (*.vbo)")]
        onAccepted: appController.loadVbo(selectedFile)
    }
    FileDialog {
        id: projectOpenDialog
        title: qsTr("Open FlappedEar project")
        nameFilters: [qsTr("FlappedEar projects (*.fetproject)")]
        onAccepted: {
            window.clearWidgetSelection();
            appController.openProject(selectedFile);
            window.welcomeVisible = false;
        }
    }
    FileDialog {
        id: projectSaveDialog
        title: qsTr("Save FlappedEar project")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "fetproject"
        nameFilters: [qsTr("FlappedEar projects (*.fetproject)")]
        onAccepted: appController.saveProject(selectedFile)
    }
    FileDialog {
        id: exportOutputDialog
        title: qsTr("Export HEVC video")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "mp4"
        nameFilters: [qsTr("HEVC MP4 video (*.mp4)")]
        onAccepted: exportDialog.outputFile = selectedFile
    }
    FileDialog {
        id: templateImportDialog
        title: qsTr("Import layout template")
        nameFilters: [qsTr("FlappedEar templates (*.fettemplate *.json)")]
        onAccepted: {
            const templateId = appController.widgetModel.importTemplate(selectedFile);
            if (templateId)
                templatePicker.currentIndex = window.templateIndexById(templateId);
        }
    }
    FileDialog {
        id: templateExportDialog
        title: qsTr("Export layout template")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "fettemplate"
        nameFilters: [qsTr("FlappedEar templates (*.fettemplate)")]
        onAccepted: {
            const item = window.selectedTemplate();
            if (item)
                appController.widgetModel.exportTemplate(item.id, selectedFile);
        }
    }

    Dialog {
        id: exportDialog
        title: qsTr("Export H.265 / HEVC")
        modal: true
        width: 470
        anchors.centerIn: parent
        property url outputFile
        background: Rectangle {
            radius: 14
            color: "#0d141d"
            border.color: "#334253"
        }
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: qsTr("Source resolution and frame rate are preserved. Audio is encoded to AAC when present.")
                color: "#8b98a8"
                wrapMode: Text.WordWrap
                font.pixelSize: 11
            }
            Label {
                Layout.fillWidth: true
                text: exportDialog.outputFile.toString().length > 0
                    ? exportDialog.outputFile.toString().replace("file://", "")
                    : qsTr("Choose output file…")
                color: exportDialog.outputFile.toString().length > 0 ? "#e8edf4" : "#718092"
                elide: Text.ElideMiddle
            }
            FeButton {
                Layout.fillWidth: true
                text: qsTr("Choose output…")
                onClicked: exportOutputDialog.open()
            }
            Label {
                text: qsTr("Quality")
                color: "#8b98a8"
                font.pixelSize: 11
            }
            FeComboBox {
                id: exportQuality
                Layout.fillWidth: true
                model: [qsTr("Fast"), qsTr("High"), qsTr("Maximum")]
                currentIndex: 1
            }
            FeCheckBox {
                id: exportAudio
                text: qsTr("Preserve audio (AAC)")
                checked: true
            }
            Label {
                visible: appController.exportState === "failed" && appController.exportError.length > 0
                Layout.fillWidth: true
                text: appController.exportError
                color: "#ff8a92"
                wrapMode: Text.WordWrap
                font.pixelSize: 11
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                FeButton {
                    text: qsTr("Cancel")
                    onClicked: exportDialog.close()
                }
                FeButton {
                    accent: true
                    text: qsTr("Export")
                    enabled: exportDialog.outputFile.toString().length > 0
                    onClicked: {
                        const quality = ["fast", "high", "maximum"][exportQuality.currentIndex];
                        appController.startExport(exportDialog.outputFile, quality, exportAudio.checked);
                        exportDialog.close();
                    }
                }
            }
        }
    }

    Popup {
        id: exportProgressPopup
        visible: appController.exporting || appController.exportState === "cancelling"
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: parent
        width: 360
        height: 172
        background: Rectangle {
            radius: 14
            color: "#0d141d"
            border.color: "#334253"
        }
        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 10
            Label {
                text: appController.exportState === "cancelling" ? qsTr("Cancelling export…") : qsTr("Exporting HEVC…")
                color: "#f2f6fb"
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: 100
                value: appController.exportProgress
            }
            Label {
                text: qsTr("%1%").arg(appController.exportProgress)
                color: "#8b98a8"
                font.pixelSize: 12
            }
            Item { Layout.fillHeight: true }
            FeButton {
                Layout.alignment: Qt.AlignRight
                text: appController.exportState === "cancelling" ? qsTr("Cancelling…") : qsTr("Cancel")
                enabled: appController.exportState !== "cancelling"
                onClicked: appController.cancelExport()
            }
        }
    }

    Popup {
        id: templateSavePopup
        x: (window.width - width) / 2
        y: (window.height - height) / 2
        width: 420
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: {
            templateName.text = "";
            templateDescription.text = "";
            templateName.forceActiveFocus();
        }
        background: Rectangle {
            radius: 14
            color: "#0d141d"
            border.color: "#334253"
        }
        contentItem: ColumnLayout {
            spacing: 10
            Label {
                text: qsTr("Save custom template")
                color: "#f1f5fa"
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Capture the complete current scene, including widget positions, bindings and styles.")
                color: "#778596"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
            Label {
                text: qsTr("Name")
                color: "#9ba7b5"
                font.pixelSize: 11
            }
            FeTextField {
                id: templateName
                Layout.fillWidth: true
                placeholderText: qsTr("My circuit layout")
            }
            Label {
                text: qsTr("Description")
                color: "#9ba7b5"
                font.pixelSize: 11
            }
            FeTextField {
                id: templateDescription
                Layout.fillWidth: true
                placeholderText: qsTr("When and why to use this layout")
            }
            RowLayout {
                Layout.fillWidth: true
                Item {
                    Layout.fillWidth: true
                }
                FeButton {
                    text: qsTr("Cancel")
                    onClicked: templateSavePopup.close()
                }
                FeButton {
                    accent: true
                    text: qsTr("Save template")
                    enabled: templateName.text.trim().length > 0 && appController.widgetModel.count > 0
                    onClicked: {
                        const templateId = appController.widgetModel.saveCurrentAsTemplate(templateName.text, templateDescription.text);
                        if (templateId) {
                            templatePicker.currentIndex = window.templateIndexById(templateId);
                            templateSavePopup.close();
                        }
                    }
                }
            }
        }
    }

    MediaPlayer {
        id: mediaPlayer
        source: appController.videoSource
        audioOutput: AudioOutput {}
        videoOutput: videoOutput
        onPositionChanged: appController.playbackTime = position / 1000.0
    }

    AnalysisWindow {
        id: analysisWindow
        videoSource: appController.videoSource
        playbackPosition: mediaPlayer.position
        playbackRunning: mediaPlayer.playbackState === MediaPlayer.PlayingState
        mediaDuration: mediaPlayer.duration
        onSeekRequested: milliseconds => mediaPlayer.position = milliseconds
        onTogglePlaybackRequested: mediaPlayer.playbackState === MediaPlayer.PlayingState ? mediaPlayer.pause() : mediaPlayer.play()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            visible: !window.fullScreenPreview
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: "#0c1118"
            border.color: "#202a36"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                spacing: 12

                Image {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    Layout.maximumWidth: 28
                    Layout.maximumHeight: 28
                    source: "qrc:/flappedear/resources/branding/app-logo.png"
                    sourceSize.width: 56
                    sourceSize.height: 56
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                }
                ColumnLayout {
                    spacing: -1
                    Label {
                        text: "FlappedEar"
                        color: "#f1f5fa"
                        font.family: "Helvetica Neue"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: "TELEMETRY STUDIO"
                        color: "#647386"
                        font.family: "Helvetica Neue"
                        font.pixelSize: 9
                        font.letterSpacing: 1.5
                    }
                }
                Rectangle {
                    width: 1
                    height: 30
                    color: "#26313e"
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8
                }
                ColumnLayout {
                    Layout.maximumWidth: 300
                    spacing: 0
                    Label {
                        text: appController.videoName || qsTr("No video selected")
                        color: appController.videoName ? "#ccd5df" : "#667486"
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                    Label {
                        text: appController.telemetryName || qsTr("No telemetry selected")
                        color: appController.telemetryName ? "#7f8e9f" : "#566373"
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
                Rectangle {
                    visible: appController.syncOffset !== 0
                    implicitWidth: syncLabel.implicitWidth + 18
                    height: 28
                    radius: 14
                    color: "#10251d"
                    border.color: "#24543f"
                    Label {
                        id: syncLabel
                        anchors.centerIn: parent
                        text: qsTr("SYNC  %1 s").arg(appController.syncOffset.toFixed(3))
                        color: "#55e6a5"
                        font.pixelSize: 9
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.7
                    }
                }
                FeButton {
                    compact: true
                    text: qsTr("Open video")
                    onClicked: videoDialog.open()
                }
                FeButton {
                    compact: true
                    text: qsTr("Open VBO")
                    onClicked: vboDialog.open()
                }
                FeButton {
                    compact: true
                    accent: true
                    text: qsTr("Export")
                    enabled: appController.videoName.length > 0 && appController.telemetryName.length > 0
                    onClicked: exportDialog.open()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                visible: !window.fullScreenPreview
                Layout.preferredWidth: 238
                Layout.fillHeight: true
                color: "#0a0f15"
                border.color: "#202a36"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: 14
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.bottomMargin: 12
                    spacing: 8

                    SectionTitle {
                        text: qsTr("Layout template")
                    }
                    FeComboBox {
                        id: templatePicker
                        Layout.fillWidth: true
                        model: window.templateNames()
                        currentIndex: 0
                    }
                    Label {
                        Layout.fillWidth: true
                        text: window.selectedTemplate() ? window.selectedTemplate().description : ""
                        color: "#657386"
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                    FeButton {
                        Layout.fillWidth: true
                        compact: true
                        text: qsTr("Apply template")
                        onClicked: {
                            const item = window.selectedTemplate();
                            if (item) {
                                window.clearWidgetSelection();
                                appController.widgetModel.applyTemplate(item.id);
                            }
                        }
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 6
                        rowSpacing: 6
                        FeButton {
                            Layout.fillWidth: true
                            compact: true
                            text: qsTr("Save current")
                            onClicked: templateSavePopup.open()
                        }
                        FeButton {
                            Layout.fillWidth: true
                            compact: true
                            text: qsTr("Import…")
                            onClicked: templateImportDialog.open()
                        }
                        FeButton {
                            Layout.fillWidth: true
                            compact: true
                            text: qsTr("Export…")
                            enabled: window.selectedTemplate() !== null
                            onClicked: templateExportDialog.open()
                        }
                        FeButton {
                            Layout.fillWidth: true
                            compact: true
                            danger: true
                            text: qsTr("Delete")
                            enabled: window.selectedTemplate() !== null && !window.selectedTemplate().builtIn
                            onClicked: {
                                const item = window.selectedTemplate();
                                if (item && appController.widgetModel.deleteTemplate(item.id))
                                    templatePicker.currentIndex = 0;
                            }
                        }
                    }

                    SectionTitle {
                        text: qsTr("Add widget")
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 7
                        rowSpacing: 7
                        Repeater {
                            model: window.widgetCatalog
                            Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: 50
                                radius: 9
                                color: addMouse.containsMouse ? "#17212c" : "#111821"
                                border.color: addMouse.containsMouse ? "#3a4b5e" : "#24303d"
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 7
                                    spacing: 7
                                    Rectangle {
                                        width: 26
                                        height: 26
                                        radius: 7
                                        color: "#172a23"
                                        Label {
                                            anchors.centerIn: parent
                                            text: modelData.icon
                                            color: "#55e6a5"
                                            font.pixelSize: modelData.icon.length > 2 ? 7 : 11
                                            font.weight: Font.Bold
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: modelData.label
                                        color: "#b9c4d0"
                                        font.pixelSize: 10
                                        wrapMode: Text.WordWrap
                                    }
                                }
                                MouseArea {
                                    id: addMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: window.selectWidget(appController.widgetModel.addWidget(modelData.type), false)
                                }
                            }
                        }
                    }

                    SectionTitle {
                        text: qsTr("Layers · %1").arg(appController.widgetModel.count)
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        FeButton {
                            Layout.fillWidth: true
                            compact: true
                            text: qsTr("Group")
                            enabled: window.selectedWidgetIndices.length >= 2
                            onClicked: window.groupSelectedWidgets()
                        }
                        FeButton {
                            Layout.fillWidth: true
                            compact: true
                            text: qsTr("Ungroup")
                            enabled: window.selectedWidgetIsGrouped()
                            onClicked: window.ungroupSelectedWidgets()
                        }
                    }
                    Label {
                        visible: window.selectedWidgetIndices.length > 1
                        Layout.fillWidth: true
                        text: qsTr("%1 layers selected").arg(window.selectedWidgetIndices.length)
                        color: "#55e6a5"
                        font.pixelSize: 9
                    }
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ColumnLayout {
                            width: 210
                            spacing: 5
                            Repeater {
                                model: appController.widgetModel
                                Rectangle {
                                    required property int index
                                    required property string widgetType
                                    required property bool widgetVisible
                                    required property var widgetSettings
                                    required property string widgetGroupId
                                    Layout.fillWidth: true
                                    height: 38
                                    radius: 8
                                    color: window.isWidgetSelected(index) ? "#16261f" : layerMouse.containsMouse ? "#131c26" : "transparent"
                                    border.color: window.isWidgetSelected(index) ? "#2d6a50" : "transparent"
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 9
                                        anchors.rightMargin: 5
                                        Label {
                                            text: window.isWidgetSelected(index) ? "●" : (widgetGroupId ? "◆" : "○")
                                            color: window.isWidgetSelected(index) ? "#55e6a5" : "#536172"
                                            font.pixelSize: 9
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: widgetSettings.name || widgetType
                                            color: widgetVisible ? "#c7d0db" : "#596575"
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                        }
                                        FeButton {
                                            width: 30
                                            implicitWidth: 30
                                            compact: true
                                            text: widgetVisible ? "◉" : "○"
                                            onClicked: appController.widgetModel.setWidgetProperty(index, "visible", !widgetVisible)
                                        }
                                    }
                                    MouseArea {
                                        id: layerMouse
                                        anchors.fill: parent
                                        anchors.rightMargin: 34
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: mouse => window.selectWidget(index, !!(mouse.modifiers & Qt.ShiftModifier))
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#06090d"
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Rectangle {
                            id: stageFrame
                            anchors.fill: parent
                            anchors.margins: window.fullScreenPreview ? 0 : 18
                            radius: window.fullScreenPreview ? 0 : 12
                            color: "#020304"
                            border.color: window.fullScreenPreview ? "transparent" : "#26313d"
                            clip: true
                            Canvas {
                                anchors.fill: parent
                                visible: !appController.videoSource.toString()
                                opacity: 0.28
                                onPaint: {
                                    const ctx = getContext("2d");
                                    ctx.reset();
                                    ctx.strokeStyle = "#26313d";
                                    ctx.lineWidth = 1;
                                    for (let x = 0; x < width; x += 32) {
                                        ctx.beginPath();
                                        ctx.moveTo(x, 0);
                                        ctx.lineTo(x, height);
                                        ctx.stroke();
                                    }
                                    for (let y = 0; y < height; y += 32) {
                                        ctx.beginPath();
                                        ctx.moveTo(0, y);
                                        ctx.lineTo(width, y);
                                        ctx.stroke();
                                    }
                                }
                            }
                            VideoOutput {
                                id: videoOutput
                                anchors.fill: parent
                                fillMode: VideoOutput.PreserveAspectFit
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.ArrowCursor
                                onClicked: window.clearWidgetSelection()
                                onDoubleClicked: window.toggleFullScreen()
                            }
                            ColumnLayout {
                                anchors.centerIn: parent
                                visible: !appController.videoSource.toString()
                                spacing: 8
                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter
                                    width: 56
                                    height: 56
                                    radius: 18
                                    color: "#111923"
                                    border.color: "#263442"
                                    Label {
                                        anchors.centerIn: parent
                                        text: "▶"
                                        color: "#55e6a5"
                                        font.pixelSize: 20
                                    }
                                }
                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: qsTr("Drop into the driver’s perspective")
                                    color: "#c2ccd7"
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }
                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: qsTr("Open an MP4 or MOV to start building the overlay")
                                    color: "#657386"
                                    font.pixelSize: 11
                                }
                                FeButton {
                                    Layout.alignment: Qt.AlignHCenter
                                    accent: true
                                    text: qsTr("Choose video")
                                    onClicked: videoDialog.open()
                                }
                            }
                            WidgetOverlay {
                                id: overlay
                                x: videoOutput.contentRect.x
                                y: videoOutput.contentRect.y
                                width: videoOutput.contentRect.width
                                height: videoOutput.contentRect.height
                                visible: width > 0 && height > 0
                                selectedIndex: window.selectedWidgetIndex
                                selectedIndices: window.selectedWidgetIndices
                                onSelectionRequested: (index, additive) => window.selectWidget(index, additive)
                                onFullScreenRequested: window.toggleFullScreen()
                            }
                        }
                    }
                    Rectangle {
                        visible: !window.fullScreenPreview
                        Layout.fillWidth: true
                        Layout.preferredHeight: 76
                        color: "#0b1017"
                        border.color: "#202a36"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 10
                            FeButton {
                                width: 40
                                implicitWidth: 40
                                height: 40
                                enabled: !!appController.videoSource.toString()
                                accent: mediaPlayer.playbackState === MediaPlayer.PlayingState
                                text: mediaPlayer.playbackState === MediaPlayer.PlayingState ? "Ⅱ" : "▶"
                                onClicked: mediaPlayer.playbackState === MediaPlayer.PlayingState ? mediaPlayer.pause() : mediaPlayer.play()
                            }
                            Label {
                                text: window.formatTime(mediaPlayer.position)
                                color: "#d2dae4"
                                font.family: "Menlo"
                                font.pixelSize: 10
                            }
                            Item {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 40
                                FeSlider {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    from: 0
                                    to: Math.max(1, mediaPlayer.duration)
                                    value: mediaPlayer.position
                                    onMoved: mediaPlayer.position = value
                                }
                                Repeater {
                                    model: window.selectedWidgetCues()
                                    Rectangle {
                                        required property var modelData
                                        x: Math.max(0, Math.min(parent.width, Number(modelData.start || 0) * 1000 / Math.max(1, mediaPlayer.duration) * parent.width))
                                        y: 1
                                        width: Math.max(3, Math.min(parent.width - x, Number(modelData.duration || 0) * 1000 / Math.max(1, mediaPlayer.duration) * parent.width))
                                        height: 4
                                        radius: 2
                                        color: "#55e6a5"
                                        opacity: 0.8
                                    }
                                }
                            }
                            Label {
                                text: window.formatTime(mediaPlayer.duration)
                                color: "#6f7e90"
                                font.family: "Menlo"
                                font.pixelSize: 10
                            }
                            FeButton {
                                compact: true
                                accent: appController.analysisVisible
                                text: qsTr("ANALYSIS")
                                onClicked: appController.analysisVisible = !appController.analysisVisible
                            }
                            FeButton {
                                width: 38
                                implicitWidth: 38
                                compact: true
                                text: "⛶"
                                onClicked: window.toggleFullScreen()
                            }
                        }
                    }
                }
            }

            InspectorPanel {
                id: inspector
                visible: !window.fullScreenPreview
                Layout.preferredWidth: 350
                Layout.fillHeight: true
                selectedIndex: window.selectedWidgetIndex
                onSelectionCleared: window.clearWidgetSelection()
                onSelectionRequested: index => window.selectWidget(index, false)
            }
        }

        Rectangle {
            visible: !window.fullScreenPreview
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: "#0a0f15"
            border.color: "#202a36"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                Label {
                    Layout.fillWidth: true
                    text: appController.statusText
                    color: "#718092"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
                Label {
                    text: qsTr("%1 widgets").arg(appController.widgetModel.count)
                    color: "#536172"
                    font.pixelSize: 9
                }
                Rectangle {
                    width: 4
                    height: 4
                    radius: 2
                    color: appController.videoName && appController.telemetryName ? "#55e6a5" : "#485565"
                }
            }
        }
    }

    Rectangle {
        id: welcome
        anchors.fill: parent
        visible: window.welcomeVisible
        z: 100
        color: "#070b10"

        Rectangle {
            anchors.fill: parent
            opacity: 0.22
            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: "#173527"
                }
                GradientStop {
                    position: 0.48
                    color: "#091018"
                }
                GradientStop {
                    position: 1
                    color: "#10243b"
                }
            }
        }

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(820, parent.width - 80)
            spacing: 20

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 12
                Image {
                    Layout.preferredWidth: 34
                    Layout.preferredHeight: 34
                    Layout.maximumWidth: 34
                    Layout.maximumHeight: 34
                    source: "qrc:/flappedear/resources/branding/app-logo.png"
                    sourceSize.width: 68
                    sourceSize.height: 68
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                }
                ColumnLayout {
                    spacing: -2
                    Label {
                        text: "FlappedEar"
                        color: "#f2f6fb"
                        font.family: "Helvetica Neue"
                        font.pixelSize: 21
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: "TELEMETRY STUDIO"
                        color: "#718092"
                        font.pixelSize: 9
                        font.letterSpacing: 2
                    }
                }
            }

            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 5
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Turn a drive into a finished telemetry overlay")
                    color: "#f2f6fb"
                    font.family: "Helvetica Neue"
                    font.pixelSize: 25
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Choose the clip and telemetry for this export. Nothing else gets in the way.")
                    color: "#8290a1"
                    font.pixelSize: 12
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 188
                    radius: 14
                    color: "#0d141d"
                    border.color: appController.videoName ? "#2c6b50" : "#263442"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 8
                        Rectangle {
                            width: 38
                            height: 38
                            radius: 11
                            color: appController.videoName ? "#173528" : "#151e29"
                            Label {
                                anchors.centerIn: parent
                                text: "▶"
                                color: appController.videoName ? "#55e6a5" : "#718092"
                                font.pixelSize: 14
                            }
                        }
                        Label {
                            text: qsTr("1 · Video clip")
                            color: "#e8edf4"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                        Label {
                            Layout.fillWidth: true
                            text: appController.videoName || qsTr("MP4 or MOV from the camera")
                            color: appController.videoName ? "#55e6a5" : "#718092"
                            elide: Text.ElideMiddle
                            font.pixelSize: 10
                        }
                        Item {
                            Layout.fillHeight: true
                        }
                        FeButton {
                            Layout.fillWidth: true
                            text: appController.videoName ? qsTr("Change video") : qsTr("Choose video")
                            accent: !appController.videoName
                            onClicked: videoDialog.open()
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 188
                    radius: 14
                    color: "#0d141d"
                    border.color: appController.telemetryName ? "#2c6b50" : "#263442"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 8
                        Rectangle {
                            width: 38
                            height: 38
                            radius: 11
                            color: appController.telemetryName ? "#173528" : "#151e29"
                            Label {
                                anchors.centerIn: parent
                                text: "⌁"
                                color: appController.telemetryName ? "#55e6a5" : "#718092"
                                font.pixelSize: 17
                            }
                        }
                        Label {
                            text: qsTr("2 · Telemetry")
                            color: "#e8edf4"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                        Label {
                            Layout.fillWidth: true
                            text: appController.telemetryName || qsTr("Optional VBOX session")
                            color: appController.telemetryName ? "#55e6a5" : "#718092"
                            elide: Text.ElideMiddle
                            font.pixelSize: 10
                        }
                        Item {
                            Layout.fillHeight: true
                        }
                        FeButton {
                            Layout.fillWidth: true
                            text: appController.telemetryName ? qsTr("Change VBO") : qsTr("Choose VBO")
                            onClicked: vboDialog.open()
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 188
                    radius: 14
                    color: "#0d141d"
                    border.color: "#263442"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 8
                        Rectangle {
                            width: 38
                            height: 38
                            radius: 11
                            color: "#151e29"
                            Label {
                                anchors.centerIn: parent
                                text: "◇"
                                color: "#42a5ff"
                                font.pixelSize: 18
                            }
                        }
                        Label {
                            text: qsTr("Saved project")
                            color: "#e8edf4"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Resume a configured v2 project")
                            color: "#718092"
                            wrapMode: Text.WordWrap
                            font.pixelSize: 10
                        }
                        Item {
                            Layout.fillHeight: true
                        }
                        FeButton {
                            Layout.fillWidth: true
                            text: qsTr("Open project…")
                            onClicked: projectOpenDialog.open()
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: appController.videoName ? qsTr("Ready to build the overlay") : qsTr("Choose a video to continue")
                    color: appController.videoName ? "#55e6a5" : "#657386"
                    font.pixelSize: 11
                }
                FeButton {
                    text: qsTr("Enter Studio  →")
                    accent: true
                    enabled: !!appController.videoName
                    onClicked: window.welcomeVisible = false
                }
            }
        }
    }
}
