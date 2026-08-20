import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property int selectedIndex: -1
    property var selectedIndices: []
    signal selectionRequested(int index, bool additive)

    Repeater {
        model: appController.widgetModel

        Item {
            id: widgetItem
            required property int index
            required property string widgetId
            required property string widgetType
            required property real widgetX
            required property real widgetY
            required property real widgetWidth
            required property real widgetHeight
            required property real widgetScale
            required property real widgetRotation
            required property real widgetOpacity
            required property bool widgetVisible
            required property var widgetSettings
            required property var widgetCues
            required property string widgetGroupId

            x: widgetX * root.width
            y: widgetY * root.height + cueYOffset
            width: widgetWidth * widgetScale * root.width
            height: widgetHeight * widgetScale * root.height
            rotation: widgetRotation
            scale: cueScale
            opacity: widgetOpacity * cueOpacity
            visible: widgetVisible && (widgetCues.length === 0 || cueOpacity > 0)

            property var activeCue: {
                appController.playbackTime;
                let best = null;
                let bestOpacity = 0;
                for (let index = 0; index < widgetCues.length; ++index) {
                    const cue = widgetCues[index];
                    const start = Number(cue.start || 0);
                    const duration = Math.max(0.1, Number(cue.duration || 0.1));
                    const elapsed = appController.playbackTime - start;
                    if (elapsed < 0 || elapsed > duration)
                        continue;
                    const fadeIn = Math.min(duration, Math.max(0, Number(cue.fadeIn || 0)));
                    const fadeOut = Math.min(duration, Math.max(0, Number(cue.fadeOut || 0)));
                    let amount = 1;
                    if (fadeIn > 0 && elapsed < fadeIn)
                        amount = elapsed / fadeIn;
                    if (fadeOut > 0 && elapsed > duration - fadeOut)
                        amount = Math.min(amount, (duration - elapsed) / fadeOut);
                    if (amount >= bestOpacity) {
                        bestOpacity = amount;
                        best = {
                            "cue": cue,
                            "amount": Math.max(0, Math.min(1, amount)),
                            "elapsed": elapsed,
                            "fadeIn": fadeIn
                        };
                    }
                }
                return best;
            }
            property real cueOpacity: widgetCues.length === 0 ? 1 : (activeCue ? activeCue.amount : 0)
            property real cueScale: activeCue && activeCue.cue.effect === "pop" ? 0.86 + 0.14 * (activeCue.fadeIn > 0 ? Math.min(1, activeCue.elapsed / activeCue.fadeIn) : 1) : 1
            property real cueYOffset: activeCue && activeCue.cue.effect === "slideUp" ? height * 0.14 * (1 - (activeCue.fadeIn > 0 ? Math.min(1, activeCue.elapsed / activeCue.fadeIn) : 1)) : 0

            property real pad: Number(widgetSettings.padding ?? 12)
            property string family: widgetSettings.fontFamily || "Helvetica Neue"
            property int weight: Number(widgetSettings.fontWeight ?? 600)
            property color primary: widgetSettings.textColor || "#f4f7fb"
            property color secondary: widgetSettings.secondaryTextColor || "#8d9aaa"
            property color accent: widgetSettings.accentColor || "#55e6a5"
            property real valueScale: Number(widgetSettings.valueFontScale ?? 1)
            property real labelScale: Number(widgetSettings.labelFontScale ?? 1)

            function raw(key, fallback) {
                appController.playbackTime;
                return appController.telemetryValue(widgetSettings[key] || fallback);
            }
            function adjusted(value) {
                if (value === undefined || value === null)
                    return undefined;
                let result = Number(value) * Number(widgetSettings.multiplier ?? 1) + Number(widgetSettings.valueOffset ?? 0);
                if (widgetSettings.clampValue)
                    result = Math.max(Number(widgetSettings.minValue ?? 0), Math.min(Number(widgetSettings.maxValue ?? 100), result));
                return result;
            }
            function numberText(value, unitFactor) {
                const adjustedValue = adjusted(value);
                if (adjustedValue === undefined || !Number.isFinite(adjustedValue))
                    return "—";
                return (widgetSettings.prefix || "") + (adjustedValue * (unitFactor || 1)).toFixed(Number(widgetSettings.decimals ?? 0)) + (widgetSettings.suffix || "");
            }

            Rectangle {
                anchors.fill: parent
                visible: widgetItem.widgetSettings.showBackground ?? true
                radius: Number(widgetItem.widgetSettings.cornerRadius ?? 14)
                color: widgetItem.widgetSettings.backgroundColor || "#0b1018"
                opacity: Number(widgetItem.widgetSettings.backgroundOpacity ?? 0.82)
            }
            Rectangle {
                anchors.fill: parent
                visible: widgetItem.widgetSettings.showBorder ?? true
                radius: Number(widgetItem.widgetSettings.cornerRadius ?? 14)
                color: "transparent"
                border.width: Number(widgetItem.widgetSettings.borderWidth ?? 1)
                border.color: widgetItem.widgetSettings.borderColor || "#314052"
                opacity: Number(widgetItem.widgetSettings.borderOpacity ?? 0.75)
            }

            Label {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.topMargin: widgetItem.pad * 0.55
                anchors.leftMargin: widgetItem.pad
                anchors.rightMargin: widgetItem.pad
                visible: (widgetItem.widgetSettings.showTitle ?? false) && !!widgetItem.widgetSettings.title
                text: widgetItem.widgetSettings.title || ""
                color: widgetItem.secondary
                font.family: widgetItem.family
                font.weight: widgetItem.weight
                font.pixelSize: Math.max(8, 10 * widgetItem.labelScale)
                font.letterSpacing: 1
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            Item {
                anchors.fill: parent
                anchors.margins: widgetItem.pad

                Image {
                    anchors.centerIn: parent
                    visible: widgetItem.widgetType === "brandLogo"
                    width: parent.width * Math.max(0.1, Math.min(1, Number(widgetItem.widgetSettings.logoScale ?? 1)))
                    height: parent.height * Math.max(0.1, Math.min(1, Number(widgetItem.widgetSettings.logoScale ?? 1)))
                    source: "qrc:/flappedear/resources/branding/app-logo.png"
                    sourceSize.width: 512
                    sourceSize.height: 512
                    fillMode: Image.PreserveAspectFit
                    opacity: Number(widgetItem.widgetSettings.logoOpacity ?? 0.85)
                    mipmap: true
                }

                Column {
                    anchors.centerIn: parent
                    visible: widgetItem.widgetType === "speed"
                    spacing: 0
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: widgetItem.widgetSettings.label || "SPEED"
                        color: widgetItem.secondary
                        font.family: widgetItem.family
                        font.weight: Font.DemiBold
                        font.pixelSize: Math.max(8, 10 * widgetItem.labelScale)
                        font.letterSpacing: 1.5
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        property bool mph: widgetItem.widgetSettings.unit === "mph"
                        text: widgetItem.numberText(widgetItem.raw("source", "speed"), mph ? 0.621371 : 1)
                        color: widgetItem.primary
                        font.family: widgetItem.family
                        font.weight: widgetItem.weight
                        font.pixelSize: Math.min(widgetItem.width * 0.36, widgetItem.height * 0.43) * widgetItem.valueScale
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: widgetItem.widgetSettings.showUnit ?? true
                        text: widgetItem.widgetSettings.unit || "km/h"
                        color: widgetItem.accent
                        font.family: widgetItem.family
                        font.weight: Font.DemiBold
                        font.pixelSize: Math.max(8, 11 * widgetItem.labelScale)
                    }
                }
                Rectangle {
                    visible: widgetItem.widgetType === "speed" && (widgetItem.widgetSettings.showGauge ?? true)
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 3
                    radius: 2
                    color: "#24303d"
                    Rectangle {
                        property real value: Number(widgetItem.adjusted(widgetItem.raw("source", "speed")) || 0)
                        width: parent.width * Math.max(0, Math.min(1, (value - Number(widgetItem.widgetSettings.minValue ?? 0)) / Math.max(1, Number(widgetItem.widgetSettings.maxValue ?? 300) - Number(widgetItem.widgetSettings.minValue ?? 0))))
                        height: parent.height
                        radius: 2
                        color: widgetItem.accent
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    visible: widgetItem.widgetType === "rpm"
                    spacing: 4
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Column {
                            Layout.alignment: Qt.AlignVCenter
                            Label {
                                text: widgetItem.widgetSettings.label || "ENGINE"
                                color: widgetItem.secondary
                                font.family: widgetItem.family
                                font.pixelSize: 9 * widgetItem.labelScale
                                font.letterSpacing: 1.2
                            }
                            Label {
                                text: widgetItem.numberText(widgetItem.raw("source", "rpm"), 1)
                                color: Number(widgetItem.adjusted(widgetItem.raw("source", "rpm")) || 0) >= Number(widgetItem.widgetSettings.warningValue ?? 6500) ? "#ff6978" : widgetItem.primary
                                font.family: widgetItem.family
                                font.weight: widgetItem.weight
                                font.pixelSize: Math.min(34, widgetItem.height * 0.42) * widgetItem.valueScale
                            }
                        }
                        Label {
                            visible: widgetItem.widgetSettings.showUnit ?? true
                            text: widgetItem.widgetSettings.unit || "RPM"
                            color: widgetItem.accent
                            font.family: widgetItem.family
                            font.pixelSize: 10 * widgetItem.labelScale
                        }
                    }
                    Rectangle {
                        visible: widgetItem.widgetSettings.showBar ?? true
                        Layout.fillWidth: true
                        height: 5
                        radius: 3
                        color: "#24303d"
                        Rectangle {
                            property real rpmValue: Number(widgetItem.adjusted(widgetItem.raw("source", "rpm")) || 0)
                            width: parent.width * Math.max(0, Math.min(1, rpmValue / Math.max(1, Number(widgetItem.widgetSettings.maxValue ?? 8000))))
                            height: parent.height
                            radius: 3
                            color: rpmValue >= Number(widgetItem.widgetSettings.warningValue ?? 6500) ? "#ff5b63" : widgetItem.accent
                        }
                    }
                }

                RowLayout {
                    anchors.centerIn: parent
                    visible: widgetItem.widgetType === "heartRate"
                    spacing: 9
                    Label {
                        visible: widgetItem.widgetSettings.showIcon ?? true
                        text: "♥"
                        color: widgetItem.accent
                        font.pixelSize: Math.min(30, widgetItem.height * 0.38)
                    }
                    Column {
                        Label {
                            text: widgetItem.widgetSettings.label || "HEART RATE"
                            color: widgetItem.secondary
                            font.family: widgetItem.family
                            font.pixelSize: 9 * widgetItem.labelScale
                            font.letterSpacing: 1
                        }
                        Row {
                            spacing: 6
                            Label {
                                text: widgetItem.numberText(widgetItem.raw("source", "heartRate"), 1)
                                color: widgetItem.primary
                                font.family: widgetItem.family
                                font.weight: widgetItem.weight
                                font.pixelSize: Math.min(28, widgetItem.height * 0.38) * widgetItem.valueScale
                            }
                            Label {
                                anchors.baseline: parent.children[0].baseline
                                visible: widgetItem.widgetSettings.showUnit ?? true
                                text: widgetItem.widgetSettings.unit || "BPM"
                                color: widgetItem.accent
                                font.family: widgetItem.family
                                font.pixelSize: 9 * widgetItem.labelScale
                            }
                        }
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    visible: widgetItem.widgetType === "pedals"
                    spacing: 7
                    Repeater {
                        model: [
                            {
                                "labelKey": "acceleratorLabel",
                                "sourceKey": "acceleratorSource",
                                "fallback": "throttle",
                                "colorKey": "acceleratorColor",
                                "minKey": "acceleratorMin",
                                "maxKey": "acceleratorMax"
                            },
                            {
                                "labelKey": "brakeLabel",
                                "sourceKey": "brakeSource",
                                "fallback": "brake",
                                "colorKey": "brakeColor",
                                "minKey": "brakeMin",
                                "maxKey": "brakeMax"
                            }
                        ]
                        ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 3
                            property var pedalRaw: widgetItem.raw(modelData.sourceKey, modelData.fallback)
                            property real pedalValue: Number(pedalRaw || 0)
                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    Layout.fillWidth: true
                                    text: widgetItem.widgetSettings[modelData.labelKey] || modelData.fallback.toUpperCase()
                                    color: widgetItem.secondary
                                    font.family: widgetItem.family
                                    font.pixelSize: 9 * widgetItem.labelScale
                                    font.letterSpacing: 0.8
                                }
                                Label {
                                    visible: widgetItem.widgetSettings.showValues ?? true
                                    text: parent.parent.pedalRaw === undefined ? "—" : parent.parent.pedalValue.toFixed(Number(widgetItem.widgetSettings.decimals ?? 0)) + "%"
                                    color: widgetItem.primary
                                    font.family: widgetItem.family
                                    font.weight: Font.DemiBold
                                    font.pixelSize: 10 * widgetItem.labelScale
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 8
                                radius: Number(widgetItem.widgetSettings.barRadius ?? 5)
                                color: "#24303d"
                                Rectangle {
                                    property real low: Number(widgetItem.widgetSettings[modelData.minKey] ?? 0)
                                    property real high: Number(widgetItem.widgetSettings[modelData.maxKey] ?? 100)
                                    width: parent.width * Math.max(0, Math.min(1, (parent.parent.pedalValue - low) / Math.max(0.001, high - low)))
                                    height: parent.height
                                    radius: parent.radius
                                    color: widgetItem.widgetSettings[modelData.colorKey] || widgetItem.accent
                                }
                            }
                        }
                    }
                }

                Item {
                    anchors.fill: parent
                    visible: widgetItem.widgetType === "gForce"
                    property real lateral: Number(widgetItem.raw("lateralSource", "lateralAcceleration") || 0)
                    property real longitudinal: Number(widgetItem.raw("longitudinalSource", "longitudinalAcceleration") || 0)
                    property real range: Math.max(0.1, Number(widgetItem.widgetSettings.gRange ?? 2))
                    Rectangle {
                        anchors.centerIn: parent
                        width: Math.min(parent.width, parent.height - 20) * 0.76
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.color: widgetItem.widgetSettings.gridColor || "#566477"
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 1
                        height: parent.height * 0.68
                        color: widgetItem.widgetSettings.gridColor || "#566477"
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.68
                        height: 1
                        color: widgetItem.widgetSettings.gridColor || "#566477"
                    }
                    Rectangle {
                        property real dot: Number(widgetItem.widgetSettings.dotSize ?? 12)
                        width: dot
                        height: dot
                        radius: dot / 2
                        color: widgetItem.accent
                        x: parent.width / 2 - width / 2 + Math.max(-1, Math.min(1, parent.lateral / parent.range)) * parent.width * 0.28
                        y: parent.height / 2 - height / 2 - Math.max(-1, Math.min(1, parent.longitudinal / parent.range)) * parent.height * 0.28
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        visible: widgetItem.widgetSettings.showCombined ?? true
                        text: Math.sqrt(parent.lateral * parent.lateral + parent.longitudinal * parent.longitudinal).toFixed(Number(widgetItem.widgetSettings.decimals ?? 2)) + " g"
                        color: widgetItem.primary
                        font.family: widgetItem.family
                        font.weight: Font.DemiBold
                        font.pixelSize: 10 * widgetItem.labelScale
                    }
                }

                Item {
                    anchors.fill: parent
                    visible: widgetItem.widgetType === "arcGauge"
                    property var gaugeRaw: widgetItem.raw("source", "speed")
                    property real gaugeValue: Number(widgetItem.adjusted(gaugeRaw) || 0)
                    property real minimum: Number(widgetItem.widgetSettings.minValue ?? 0)
                    property real maximum: Math.max(minimum + 0.001, Number(widgetItem.widgetSettings.maxValue ?? 300))
                    property real progress: Math.max(0, Math.min(1, (gaugeValue - minimum) / (maximum - minimum)))
                    Canvas {
                        id: arcCanvas
                        anchors.fill: parent
                        property real progress: parent.progress
                        property real startAngle: Number(widgetItem.widgetSettings.startAngle ?? 155)
                        property real endAngle: Number(widgetItem.widgetSettings.endAngle ?? 385)
                        property real arcWidth: Number(widgetItem.widgetSettings.arcWidth ?? 12)
                        onProgressChanged: requestPaint()
                        onStartAngleChanged: requestPaint()
                        onEndAngleChanged: requestPaint()
                        onArcWidthChanged: requestPaint()
                        onPaint: {
                            const context = getContext("2d");
                            context.reset();
                            const start = startAngle * Math.PI / 180;
                            const end = endAngle * Math.PI / 180;
                            const radius = Math.max(4, Math.min(width, height) * 0.38);
                            const centerX = width / 2;
                            const centerY = height * 0.52;
                            context.lineCap = "round";
                            context.lineWidth = arcWidth;
                            context.strokeStyle = widgetItem.widgetSettings.trackColor || "#263442";
                            context.beginPath();
                            context.arc(centerX, centerY, radius, start, end, false);
                            context.stroke();
                            context.strokeStyle = widgetItem.accent;
                            context.beginPath();
                            context.arc(centerX, centerY, radius, start, start + (end - start) * progress, false);
                            context.stroke();
                        }
                    }
                    Column {
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: parent.height * 0.08
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: widgetItem.widgetSettings.showValue ?? true
                            text: widgetItem.numberText(parent.parent.gaugeRaw, 1)
                            color: widgetItem.primary
                            font.family: widgetItem.family
                            font.weight: widgetItem.weight
                            font.pixelSize: Math.min(34, widgetItem.height * 0.24) * widgetItem.valueScale
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: (widgetItem.widgetSettings.label || "GAUGE") + ((widgetItem.widgetSettings.showUnit ?? true) ? "  " + (widgetItem.widgetSettings.unit || "") : "")
                            color: widgetItem.secondary
                            font.family: widgetItem.family
                            font.pixelSize: 9 * widgetItem.labelScale
                            font.letterSpacing: 1
                        }
                    }
                    RowLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        visible: widgetItem.widgetSettings.showMinMax ?? true
                        Label {
                            text: parent.parent.minimum.toFixed(0)
                            color: widgetItem.secondary
                            font.pixelSize: 8
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Label {
                            text: parent.parent.maximum.toFixed(0)
                            color: widgetItem.secondary
                            font.pixelSize: 8
                        }
                    }
                }

                Item {
                    anchors.fill: parent
                    visible: widgetItem.widgetType === "dialGauge"
                    property var gaugeRaw: widgetItem.raw("source", "rpm")
                    property real gaugeValue: Number(widgetItem.adjusted(gaugeRaw) || 0)
                    property real minimum: Number(widgetItem.widgetSettings.minValue ?? 0)
                    property real maximum: Math.max(minimum + 0.001, Number(widgetItem.widgetSettings.maxValue ?? 8000))
                    Canvas {
                        id: dialCanvas
                        anchors.fill: parent
                        property real gaugeValue: parent.gaugeValue
                        property real minimum: parent.minimum
                        property real maximum: parent.maximum
                        property real startAngle: Number(widgetItem.widgetSettings.startAngle ?? 140)
                        property real endAngle: Number(widgetItem.widgetSettings.endAngle ?? 400)
                        property int majorTicks: Math.max(2, Number(widgetItem.widgetSettings.majorTicks ?? 8))
                        property int minorTicks: Math.max(0, Number(widgetItem.widgetSettings.minorTicks ?? 4))
                        onGaugeValueChanged: requestPaint()
                        onMinimumChanged: requestPaint()
                        onMaximumChanged: requestPaint()
                        onStartAngleChanged: requestPaint()
                        onEndAngleChanged: requestPaint()
                        onMajorTicksChanged: requestPaint()
                        onMinorTicksChanged: requestPaint()
                        onPaint: {
                            const context = getContext("2d");
                            context.reset();
                            const cx = width / 2;
                            const cy = height * 0.5;
                            const radius = Math.max(6, Math.min(width, height) * 0.38);
                            const start = startAngle * Math.PI / 180;
                            const end = endAngle * Math.PI / 180;
                            const totalTicks = (majorTicks - 1) * (minorTicks + 1);
                            if (widgetItem.widgetSettings.showTicks ?? true) {
                                context.strokeStyle = widgetItem.widgetSettings.tickColor || "#8290a0";
                                context.lineCap = "round";
                                for (let tick = 0; tick <= totalTicks; ++tick) {
                                    const angle = start + (end - start) * tick / totalTicks;
                                    const major = tick % (minorTicks + 1) === 0;
                                    const outer = radius;
                                    const inner = radius - (major ? 11 : 6);
                                    context.lineWidth = major ? 2 : 1;
                                    context.beginPath();
                                    context.moveTo(cx + Math.cos(angle) * inner, cy + Math.sin(angle) * inner);
                                    context.lineTo(cx + Math.cos(angle) * outer, cy + Math.sin(angle) * outer);
                                    context.stroke();
                                }
                            }
                            const progress = Math.max(0, Math.min(1, (gaugeValue - minimum) / (maximum - minimum)));
                            const needleAngle = start + (end - start) * progress;
                            context.strokeStyle = widgetItem.widgetSettings.needleColor || "#ff5b63";
                            context.lineWidth = 3;
                            context.beginPath();
                            context.moveTo(cx - Math.cos(needleAngle) * radius * 0.12, cy - Math.sin(needleAngle) * radius * 0.12);
                            context.lineTo(cx + Math.cos(needleAngle) * radius * 0.72, cy + Math.sin(needleAngle) * radius * 0.72);
                            context.stroke();
                            context.fillStyle = widgetItem.widgetSettings.needleColor || "#ff5b63";
                            context.beginPath();
                            context.arc(cx, cy, 5, 0, Math.PI * 2, false);
                            context.fill();
                        }
                    }
                    Column {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: widgetItem.widgetSettings.showValue ?? true
                            text: widgetItem.numberText(parent.parent.gaugeRaw, 1)
                            color: widgetItem.primary
                            font.family: widgetItem.family
                            font.weight: widgetItem.weight
                            font.pixelSize: Math.min(26, widgetItem.height * 0.18) * widgetItem.valueScale
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: (widgetItem.widgetSettings.label || "GAUGE") + ((widgetItem.widgetSettings.showUnit ?? true) ? "  " + (widgetItem.widgetSettings.unit || "") : "")
                            color: widgetItem.secondary
                            font.family: widgetItem.family
                            font.pixelSize: 8 * widgetItem.labelScale
                            font.letterSpacing: 0.8
                        }
                    }
                }

                GridLayout {
                    anchors.fill: parent
                    visible: widgetItem.widgetType === "telemetryOverlay"
                    columns: Math.max(1, Math.min(4, Number(widgetItem.widgetSettings.columns ?? 4)))
                    columnSpacing: 0
                    rowSpacing: 0
                    Repeater {
                        model: 4
                        Item {
                            required property int index
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            property int slot: index + 1
                            property var slotRaw: widgetItem.raw("source" + slot, ["speed", "rpm", "throttle", "brake"][index])
                            Rectangle {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                width: 1
                                height: parent.height * 0.62
                                visible: (widgetItem.widgetSettings.showSeparators ?? true) && parent.index < 3
                                color: widgetItem.widgetSettings.separatorColor || "#314052"
                            }
                            Column {
                                anchors.centerIn: parent
                                Label {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: widgetItem.widgetSettings["label" + parent.parent.slot] || "VALUE"
                                    color: widgetItem.secondary
                                    font.family: widgetItem.family
                                    font.pixelSize: 8 * widgetItem.labelScale
                                    font.letterSpacing: 0.8
                                }
                                Label {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: parent.parent.slotRaw === undefined ? "—" : Number(parent.parent.slotRaw).toFixed(Number(widgetItem.widgetSettings["decimals" + parent.parent.slot] ?? 0))
                                    color: widgetItem.primary
                                    font.family: widgetItem.family
                                    font.weight: widgetItem.weight
                                    font.pixelSize: Math.min(22, widgetItem.height * 0.25) * widgetItem.valueScale
                                }
                                Label {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: widgetItem.widgetSettings["unit" + parent.parent.slot] || ""
                                    color: widgetItem.accent
                                    font.family: widgetItem.family
                                    font.pixelSize: 8 * widgetItem.labelScale
                                }
                            }
                        }
                    }
                }

                Item {
                    anchors.fill: parent
                    visible: widgetItem.widgetType === "retroGrandPrix"
                    property real rpmValue: Number(widgetItem.raw("rpmSource", "rpm") || 0)
                    property real speedValue: Number(widgetItem.raw("speedSource", "speed") || 0)
                    property real gearValue: Number(widgetItem.raw("gearSource", "gear"))
                    property real throttleValue: Number(widgetItem.raw("throttleSource", "throttle") || 0)
                    property real brakeValue: Number(widgetItem.raw("brakeSource", "brake") || 0)
                    property var timingValue: widgetItem.raw("timingSource", "")
                    Canvas {
                        id: retroCanvas
                        anchors.fill: parent
                        property real rpmValue: parent.rpmValue
                        property real speedValue: parent.speedValue
                        property real gearValue: parent.gearValue
                        property real throttleValue: parent.throttleValue
                        property real brakeValue: parent.brakeValue
                        property var timingValue: parent.timingValue
                        onRpmValueChanged: requestPaint()
                        onSpeedValueChanged: requestPaint()
                        onGearValueChanged: requestPaint()
                        onThrottleValueChanged: requestPaint()
                        onBrakeValueChanged: requestPaint()
                        onTimingValueChanged: requestPaint()
                        onPaint: {
                            const ctx = getContext("2d");
                            ctx.reset();
                            ctx.save();
                            ctx.scale(width / 440, height / 420);
                            const settings = widgetItem.widgetSettings;
                            const family = settings.fontFamily || "Arial Narrow";
                            const white = settings.dialColor || "#f4f4f4";
                            const panel = settings.panelColor || "#111111";
                            const panelOpacity = Number(settings.panelOpacity ?? 0.58);
                            const rpmMin = Number(settings.rpmMin ?? 0);
                            const rpmMax = Math.max(rpmMin + 1, Number(settings.rpmMax ?? 8000));
                            const rpmProgress = Math.max(0, Math.min(1, (rpmValue - rpmMin) / (rpmMax - rpmMin)));

                            // Translucent round backing and the double white tachometer ring.
                            ctx.globalAlpha = panelOpacity;
                            ctx.fillStyle = panel;
                            ctx.beginPath();
                            ctx.arc(160, 142, 126, 0, Math.PI * 2);
                            ctx.fill();
                            ctx.globalAlpha = 1;
                            ctx.strokeStyle = white;
                            ctx.lineWidth = 4;
                            for (const radius of [89, 98]) {
                                ctx.beginPath();
                                ctx.arc(160, 142, radius, Math.PI * 0.50, Math.PI * 2.0);
                                ctx.stroke();
                            }
                            ctx.textAlign = "center";
                            ctx.textBaseline = "middle";
                            ctx.fillStyle = white;
                            ctx.font = "700 18px " + family;
                            const rpmSteps = Math.max(4, Math.min(16, Math.round((rpmMax - rpmMin) / 1000)));
                            for (let step = 0; step <= rpmSteps; ++step) {
                                const angle = Math.PI * 0.5 + Math.PI * 1.5 * step / rpmSteps;
                                const major = true;
                                ctx.lineWidth = 2;
                                ctx.beginPath();
                                ctx.moveTo(160 + Math.cos(angle) * 101, 142 + Math.sin(angle) * 101);
                                ctx.lineTo(160 + Math.cos(angle) * 111, 142 + Math.sin(angle) * 111);
                                ctx.stroke();
                                const label = Math.round((rpmMin + (rpmMax - rpmMin) * step / rpmSteps) / 1000);
                                ctx.fillText(String(label), 160 + Math.cos(angle) * 125, 142 + Math.sin(angle) * 125);
                            }
                            const needleAngle = Math.PI * 0.5 + Math.PI * 1.5 * rpmProgress;
                            ctx.strokeStyle = settings.needleColor || "#d73737";
                            ctx.lineWidth = 5;
                            ctx.beginPath();
                            ctx.moveTo(160 - Math.cos(needleAngle) * 15, 142 - Math.sin(needleAngle) * 15);
                            ctx.lineTo(160 + Math.cos(needleAngle) * 86, 142 + Math.sin(needleAngle) * 86);
                            ctx.stroke();
                            ctx.fillStyle = white;
                            ctx.beginPath();
                            ctx.arc(160, 142, 12, 0, Math.PI * 2);
                            ctx.fill();

                            // Gear and pedal-state stack.
                            const boxX = 212;
                            const boxW = 132;
                            const boxH = 34;
                            ctx.globalAlpha = 0.96;
                            ctx.fillStyle = "#f4f4f4";
                            ctx.fillRect(boxX, 143, boxW, boxH);
                            ctx.fillStyle = "#111111";
                            ctx.font = "700 20px " + family;
                            const gear = Number.isFinite(gearValue) ? Math.round(gearValue).toString() : "—";
                            ctx.fillText((settings.gearLabel || "Gear") + "  " + gear, boxX + boxW / 2, 160);
                            const throttleProgress = Math.max(0, Math.min(1, throttleValue / 100));
                            ctx.globalAlpha = 0.45 + throttleProgress * 0.55;
                            ctx.fillStyle = settings.throttleColor || "#00c839";
                            ctx.fillRect(boxX, 177, boxW, boxH);
                            ctx.globalAlpha = 1;
                            ctx.fillStyle = white;
                            ctx.fillText(settings.throttleLabel || "Throttle", boxX + boxW / 2, 194);
                            const brakeProgress = Math.max(0, Math.min(1, brakeValue / 100));
                            ctx.globalAlpha = 0.5 + brakeProgress * 0.5;
                            ctx.fillStyle = brakeProgress > 0.03 ? (settings.brakeActiveColor || "#d23737") : (settings.brakeColor || "#575244");
                            ctx.fillRect(boxX, 211, boxW, boxH);
                            ctx.globalAlpha = 1;
                            ctx.fillStyle = white;
                            ctx.fillText(settings.brakeLabel || "Brake", boxX + boxW / 2, 228);

                            // Segmented speed arc.
                            const speedMax = Math.max(1, Number(settings.speedMax ?? 360));
                            const speedProgress = Math.max(0, Math.min(1, speedValue / speedMax));
                            const speedStart = Math.PI * 0.94;
                            const speedEnd = Math.PI * 1.79;
                            const segments = 19;
                            ctx.lineWidth = 13;
                            for (let segment = 0; segment < segments; ++segment) {
                                const progress = segment / (segments - 1);
                                const a0 = speedStart + (speedEnd - speedStart) * segment / segments;
                                const a1 = speedStart + (speedEnd - speedStart) * (segment + 0.72) / segments;
                                if (progress <= speedProgress) {
                                    ctx.strokeStyle = progress < 0.58 ? (settings.speedLowColor || "#00bd31") : (progress < 0.82 ? (settings.speedMidColor || "#f2e920") : (settings.speedHighColor || "#ff9124"));
                                    ctx.globalAlpha = 1;
                                } else {
                                    ctx.strokeStyle = "#d8d8d8";
                                    ctx.globalAlpha = 0.24;
                                }
                                ctx.beginPath();
                                ctx.arc(115, 318, 120, a0, a1);
                                ctx.stroke();
                            }
                            ctx.globalAlpha = 1;
                            ctx.fillStyle = white;
                            ctx.font = "700 17px " + family;
                            ctx.textAlign = "left";
                            ctx.fillText(Math.round(speedValue) + " Km/h", 18, 354);
                            ctx.textAlign = "center";
                            ctx.fillText("200", 196, 356);
                            ctx.fillText("260", 252, 320);
                            ctx.fillText("320", 281, 275);
                            ctx.fillText("360", 292, 239);

                            // Driver and timing lower-third.
                            const plateX = 205;
                            const plateY = 348;
                            const plateW = 220;
                            const plateH = 62;
                            const gradient = ctx.createLinearGradient(plateX, plateY, plateX, plateY + 28);
                            gradient.addColorStop(0, "#ffffff");
                            gradient.addColorStop(0.55, "#c8c8cf");
                            gradient.addColorStop(1, "#f7f7f7");
                            ctx.fillStyle = gradient;
                            ctx.fillRect(plateX, plateY, plateW, 29);
                            ctx.fillStyle = "#050505";
                            ctx.fillRect(plateX, plateY + 29, plateW, plateH - 29);
                            ctx.font = "700 19px " + family;
                            ctx.fillStyle = "#111111";
                            ctx.fillText(settings.driverName || "DRIVER", plateX + plateW / 2, plateY + 15);
                            ctx.fillStyle = white;
                            ctx.textAlign = "right";
                            const timing = timingValue === undefined || timingValue === null ? (settings.timingText || "") : Number(timingValue).toFixed(Number(settings.timingDecimals ?? 1));
                            ctx.fillText(timing, plateX + plateW - 14, plateY + 46);
                            ctx.restore();
                        }
                    }
                }

                Item {
                    anchors.fill: parent
                    visible: widgetItem.widgetType === "retroTachometer"
                    property real value: Number(widgetItem.adjusted(widgetItem.raw("source", "rpm")) || 0)
                    Canvas {
                        id: retroTachometerCanvas
                        anchors.fill: parent
                        property real value: parent.value
                        onValueChanged: requestPaint()
                        onPaint: {
                            const ctx = getContext("2d");
                            ctx.reset();
                            const settings = widgetItem.widgetSettings;
                            const cx = width / 2;
                            const cy = height * 0.52;
                            const radius = Math.min(width, height) * 0.31;
                            const minimum = Number(settings.minValue ?? 0);
                            const maximum = Math.max(minimum + 1, Number(settings.maxValue ?? 8000));
                            const steps = Math.max(4, Math.min(16, Math.round((maximum - minimum) / 1000)));
                            const progress = Math.max(0, Math.min(1, (value - minimum) / (maximum - minimum)));
                            ctx.globalAlpha = Number(settings.panelOpacity ?? 0.58);
                            ctx.fillStyle = settings.panelColor || "#111111";
                            ctx.beginPath();
                            ctx.arc(cx, cy, radius * 1.30, 0, Math.PI * 2);
                            ctx.fill();
                            ctx.globalAlpha = 1;
                            ctx.strokeStyle = settings.dialColor || "#f4f4f4";
                            ctx.fillStyle = settings.dialColor || "#f4f4f4";
                            ctx.textAlign = "center";
                            ctx.textBaseline = "middle";
                            ctx.font = "700 " + Math.max(9, radius * 0.18) + "px " + widgetItem.family;
                            for (const ring of [0.92, 1.0]) {
                                ctx.lineWidth = Math.max(2, radius * 0.035);
                                ctx.beginPath();
                                ctx.arc(cx, cy, radius * ring, Math.PI * 0.5, Math.PI * 2);
                                ctx.stroke();
                            }
                            for (let step = 0; step <= steps; ++step) {
                                const angle = Math.PI * 0.5 + Math.PI * 1.5 * step / steps;
                                ctx.lineWidth = Math.max(1, radius * 0.025);
                                ctx.beginPath();
                                ctx.moveTo(cx + Math.cos(angle) * radius * 1.03, cy + Math.sin(angle) * radius * 1.03);
                                ctx.lineTo(cx + Math.cos(angle) * radius * 1.14, cy + Math.sin(angle) * radius * 1.14);
                                ctx.stroke();
                                ctx.fillText(String(Math.round((minimum + (maximum - minimum) * step / steps) / 1000)), cx + Math.cos(angle) * radius * 1.35, cy + Math.sin(angle) * radius * 1.35);
                            }
                            const angle = Math.PI * 0.5 + Math.PI * 1.5 * progress;
                            ctx.strokeStyle = settings.needleColor || "#e32636";
                            ctx.lineWidth = Math.max(3, radius * 0.05);
                            ctx.beginPath();
                            ctx.moveTo(cx - Math.cos(angle) * radius * 0.13, cy - Math.sin(angle) * radius * 0.13);
                            ctx.lineTo(cx + Math.cos(angle) * radius * 0.86, cy + Math.sin(angle) * radius * 0.86);
                            ctx.stroke();
                            ctx.fillStyle = settings.dialColor || "#f4f4f4";
                            ctx.beginPath();
                            ctx.arc(cx, cy, Math.max(5, radius * 0.12), 0, Math.PI * 2);
                            ctx.fill();
                        }
                    }
                }

                Item {
                    anchors.fill: parent
                    visible: widgetItem.widgetType === "retroGear"
                    property var rawValue: widgetItem.raw("source", "")
                    property var formattedValue: {
                        const adjusted = widgetItem.adjusted(rawValue);
                        if (adjusted === undefined || adjusted === null || !Number.isFinite(Number(adjusted)))
                            return widgetItem.widgetSettings.fallbackText ?? "—";
                        return (widgetItem.widgetSettings.prefix || "") + Number(adjusted).toFixed(Number(widgetItem.widgetSettings.decimals ?? 0)) + (widgetItem.widgetSettings.suffix || "");
                    }
                    Rectangle {
                        anchors.fill: parent
                        color: widgetItem.widgetSettings.panelColor || "#f4f4f4"
                    }
                    Label {
                        anchors.centerIn: parent
                        text: {
                            const label = widgetItem.widgetSettings.label ?? "Gear";
                            const unit = (widgetItem.widgetSettings.showUnit ?? true) ? (widgetItem.widgetSettings.unit || "") : "";
                            return [label, parent.formattedValue, unit].filter(part => String(part).length > 0).join("  ");
                        }
                        color: widgetItem.widgetSettings.valueColor || "#111111"
                        font.family: widgetItem.family
                        font.weight: Font.Bold
                        font.pixelSize: Math.max(9, Math.min(parent.height * 0.52, parent.width * 0.12))
                    }
                }

                Item {
                    anchors.fill: parent
                    visible: widgetItem.widgetType === "retroPedal"
                    property var rawValue: widgetItem.raw("source", "")
                    property real value: Number(widgetItem.adjusted(rawValue) || 0)
                    property real minimum: Number(widgetItem.widgetSettings.minValue ?? 0)
                    property real maximum: Math.max(minimum + 0.001, Number(widgetItem.widgetSettings.maxValue ?? 100))
                    property real progress: Math.max(0, Math.min(1, (value - minimum) / (maximum - minimum)))
                    Rectangle {
                        anchors.fill: parent
                        color: widgetItem.widgetSettings.emptyColor || "#3d433c"
                        opacity: 0.88
                    }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: parent.width * parent.progress
                        color: widgetItem.widgetSettings.fillColor || "#00c839"
                    }
                    Label {
                        anchors.centerIn: parent
                        text: (widgetItem.widgetSettings.label || "Pedal") + ((widgetItem.widgetSettings.showValue ?? false) ? "  " + parent.value.toFixed(Number(widgetItem.widgetSettings.decimals ?? 0)) + "%" : "")
                        color: widgetItem.primary
                        font.family: widgetItem.family
                        font.weight: Font.Bold
                        font.pixelSize: Math.max(9, Math.min(parent.height * 0.50, parent.width * 0.12))
                    }
                }

                Item {
                    anchors.fill: parent
                    visible: widgetItem.widgetType === "retroSpeedArc"
                    property real value: Number(widgetItem.adjusted(widgetItem.raw("source", "speed")) || 0)
                    Canvas {
                        id: retroSpeedCanvas
                        anchors.fill: parent
                        property real value: parent.value
                        onValueChanged: requestPaint()
                        onPaint: {
                            const ctx = getContext("2d");
                            ctx.reset();
                            const settings = widgetItem.widgetSettings;
                            const minimum = Number(settings.minValue ?? 0);
                            const maximum = Math.max(minimum + 1, Number(settings.maxValue ?? 360));
                            const progress = Math.max(0, Math.min(1, (value - minimum) / (maximum - minimum)));
                            const segments = Math.max(5, Math.min(40, Number(settings.segments ?? 19)));
                            const cx = width * 0.28;
                            const cy = height * 0.88;
                            const radius = Math.min(width * 0.50, height * 0.78);
                            const start = Math.PI * 0.92;
                            const end = Math.PI * 1.82;
                            ctx.lineWidth = Math.max(5, radius * 0.12);
                            for (let segment = 0; segment < segments; ++segment) {
                                const fraction = segment / (segments - 1);
                                const a0 = start + (end - start) * segment / segments;
                                const a1 = start + (end - start) * (segment + 0.72) / segments;
                                if (fraction <= progress) {
                                    ctx.strokeStyle = fraction < 0.58 ? (settings.lowColor || "#00bd31") : (fraction < 0.82 ? (settings.midColor || "#f2e920") : (settings.highColor || "#ff9124"));
                                    ctx.globalAlpha = 1;
                                } else {
                                    ctx.strokeStyle = settings.emptyColor || "#d8d8d8";
                                    ctx.globalAlpha = 0.25;
                                }
                                ctx.beginPath();
                                ctx.arc(cx, cy, radius, a0, a1);
                                ctx.stroke();
                            }
                            ctx.globalAlpha = 1;
                        }
                    }
                    Label {
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        text: Math.round(parent.value) + " " + (widgetItem.widgetSettings.unit || "Km/h")
                        color: widgetItem.primary
                        font.family: widgetItem.family
                        font.weight: Font.Bold
                        font.pixelSize: Math.max(9, Math.min(parent.height * 0.12, parent.width * 0.08))
                    }
                }

                Item {
                    anchors.fill: parent
                    visible: widgetItem.widgetType === "retroNameplate"
                    property var topValue: widgetItem.raw("topSource", "")
                    property var bottomValue: widgetItem.raw("bottomSource", "")
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: parent.height * 0.48
                        gradient: Gradient {
                            GradientStop {
                                position: 0
                                color: "#ffffff"
                            }
                            GradientStop {
                                position: 0.55
                                color: "#bfc0c7"
                            }
                            GradientStop {
                                position: 1
                                color: "#f7f7f7"
                            }
                        }
                    }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.topMargin: parent.height * 0.48
                        anchors.bottom: parent.bottom
                        color: "#050505"
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        height: parent.height * 0.48
                        verticalAlignment: Text.AlignVCenter
                        text: parent.topValue === undefined || parent.topValue === null ? (widgetItem.widgetSettings.topText || "") : Number(parent.topValue).toFixed(Number(widgetItem.widgetSettings.topDecimals ?? 0))
                        color: widgetItem.widgetSettings.topColor || "#111111"
                        font.family: widgetItem.family
                        font.weight: Font.Bold
                        font.pixelSize: Math.max(9, parent.height * 0.25)
                    }
                    Label {
                        anchors.right: parent.right
                        anchors.rightMargin: parent.width * 0.06
                        anchors.bottom: parent.bottom
                        height: parent.height * 0.52
                        verticalAlignment: Text.AlignVCenter
                        text: parent.bottomValue === undefined || parent.bottomValue === null ? (widgetItem.widgetSettings.bottomText || "") : Number(parent.bottomValue).toFixed(Number(widgetItem.widgetSettings.bottomDecimals ?? 1))
                        color: widgetItem.widgetSettings.bottomColor || "#f4f4f4"
                        font.family: widgetItem.family
                        font.weight: Font.Bold
                        font.pixelSize: Math.max(9, parent.height * 0.26)
                    }
                }

                Column {
                    anchors.centerIn: parent
                    visible: widgetItem.widgetType === "customValue"
                    spacing: 2
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: widgetItem.widgetSettings.label || widgetItem.widgetSettings.source || "VALUE"
                        color: widgetItem.secondary
                        font.family: widgetItem.family
                        font.pixelSize: 9 * widgetItem.labelScale
                        font.letterSpacing: 1.2
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: widgetItem.numberText(widgetItem.raw("source", ""), 1)
                        color: widgetItem.primary
                        font.family: widgetItem.family
                        font.weight: widgetItem.weight
                        font.pixelSize: Math.min(38, widgetItem.height * 0.36) * widgetItem.valueScale
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: widgetItem.widgetSettings.showUnit ?? true
                        text: widgetItem.widgetSettings.unit || ""
                        color: widgetItem.accent
                        font.family: widgetItem.family
                        font.pixelSize: 10 * widgetItem.labelScale
                    }
                }

                Item {
                    anchors.fill: parent
                    visible: widgetItem.widgetType === "track"
                    property var currentPoint: {
                        appController.playbackTime;
                        return appController.currentTrackPoint;
                    }
                    property real trackPad: Number(widgetItem.widgetSettings.trackPadding ?? 10)
                    Canvas {
                        id: trackCanvas
                        anchors.fill: parent
                        onPaint: {
                            const context = getContext("2d");
                            context.reset();
                            const points = appController.trackPoints;
                            if (points.length < 2)
                                return;
                            const pad = parent.trackPad;
                            const drawX = value => pad + (widgetItem.widgetSettings.mirrorX ? 1 - value : value) * Math.max(1, width - 2 * pad);
                            const drawY = value => pad + (widgetItem.widgetSettings.mirrorY ? 1 - value : value) * Math.max(1, height - 2 * pad);
                            context.strokeStyle = widgetItem.widgetSettings.lineColor || widgetItem.accent;
                            context.lineWidth = Number(widgetItem.widgetSettings.lineWidth ?? 3);
                            context.lineCap = "round";
                            context.lineJoin = "round";
                            context.beginPath();
                            context.moveTo(drawX(points[0].x), drawY(points[0].y));
                            for (let pointIndex = 1; pointIndex < points.length; ++pointIndex)
                                context.lineTo(drawX(points[pointIndex].x), drawY(points[pointIndex].y));
                            context.stroke();
                        }
                        Connections {
                            target: appController
                            function onTelemetryChanged() {
                                trackCanvas.requestPaint();
                            }
                        }
                    }
                    Rectangle {
                        visible: parent.currentPoint.x !== undefined
                        width: Number(widgetItem.widgetSettings.markerSize ?? 10)
                        height: width
                        radius: width / 2
                        color: widgetItem.widgetSettings.markerColor || "#ffffff"
                        x: parent.trackPad + (widgetItem.widgetSettings.mirrorX ? 1 - Number(parent.currentPoint.x || 0) : Number(parent.currentPoint.x || 0)) * Math.max(1, parent.width - 2 * parent.trackPad) - width / 2
                        y: parent.trackPad + (widgetItem.widgetSettings.mirrorY ? 1 - Number(parent.currentPoint.y || 0) : Number(parent.currentPoint.y || 0)) * Math.max(1, parent.height - 2 * parent.trackPad) - height / 2
                    }
                }
            }

            Connections {
                target: appController.widgetModel
                function onRevisionChanged() {
                    arcCanvas.requestPaint();
                    dialCanvas.requestPaint();
                    retroCanvas.requestPaint();
                    retroTachometerCanvas.requestPaint();
                    retroSpeedCanvas.requestPaint();
                    trackCanvas.requestPaint();
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: root.selectedIndices.indexOf(widgetItem.index) >= 0
                radius: Number(widgetItem.widgetSettings.cornerRadius ?? 14) + 2
                color: "transparent"
                border.width: 2
                border.color: "#55e6a5"
                opacity: 0.95
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeAllCursor
                drag.target: widgetItem
                drag.minimumX: 0
                drag.minimumY: 0
                drag.maximumX: root.width - widgetItem.width
                drag.maximumY: root.height - widgetItem.height
                onPressed: mouse => root.selectionRequested(widgetItem.index, !!(mouse.modifiers & Qt.ShiftModifier))
                onReleased: appController.widgetModel.moveWidget(widgetItem.index, widgetItem.x / root.width, widgetItem.y / root.height)
            }
            Rectangle {
                width: 14
                height: 14
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: -6
                visible: root.selectedIndex === widgetItem.index
                color: "#55e6a5"
                radius: 7
                border.color: "#07140f"
                z: 3
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.SizeFDiagCursor
                    property real startX
                    property real startY
                    property real startWidth
                    property real startHeight
                    onPressed: mouse => {
                        startX = mouse.x;
                        startY = mouse.y;
                        startWidth = widgetItem.width;
                        startHeight = widgetItem.height;
                    }
                    onPositionChanged: mouse => {
                        if (!pressed)
                            return;
                        widgetItem.width = Math.max(36, startWidth + mouse.x - startX);
                        widgetItem.height = Math.max(28, startHeight + mouse.y - startY);
                    }
                    onReleased: appController.widgetModel.resizeWidget(widgetItem.index, widgetItem.width / (root.width * widgetItem.widgetScale), widgetItem.height / (root.height * widgetItem.widgetScale))
                }
            }
        }
    }
}
