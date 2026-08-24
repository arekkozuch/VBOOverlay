import QtQuick

// A deterministic, production-QML visual acceptance scene for the modern
// motorsport broadcast HUD. It intentionally uses sample values and a bright
// roadway backdrop so panel translucency and legibility can be reviewed without
// importing private user media. Supplying both properties renders the same
// composition with a real render context and widget model instead.
Item {
    id: root
    property var renderContext: null
    property var widgetModel: null
    property bool darkBackground: false
    readonly property var activeRenderContext: root.renderContext || sampleContext
    readonly property var activeWidgetModel: root.widgetModel || smokeWidgets

    // Declare the stand-alone model before TelemetryScene binds to it. This is
    // relevant only for the deterministic acceptance scene; application
    // preview/export provide their WidgetModel explicitly.
    ListModel {
        id: smokeWidgets
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.darkBackground ? "#07111b" : "#b9dcf0" }
            GradientStop { position: 0.42; color: root.darkBackground ? "#17242d" : "#e5f0e6" }
            GradientStop { position: 0.421; color: root.darkBackground ? "#11191f" : "#4b5a58" }
            GradientStop { position: 1.0; color: root.darkBackground ? "#04080b" : "#1c252c" }
        }
    }
    Rectangle {
        width: parent.width
        height: parent.height * 0.035
        y: parent.height * 0.37
        color: root.darkBackground ? "#09120f" : "#27483b"
        opacity: 0.75
    }
    Rectangle {
        width: parent.width * 0.55
        height: parent.height * 1.25
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.36
        rotation: 13
        transformOrigin: Item.Top
        color: root.darkBackground ? "#0b1014" : "#303d43"
        opacity: root.darkBackground ? 0.94 : 0.84
    }
    Repeater {
        model: 7
        Rectangle {
            required property int index
            width: root.width * (0.012 + index * 0.006)
            height: root.height * (0.055 + index * 0.015)
            x: root.width * 0.49 + index * root.width * 0.008
            y: root.height * (0.46 + index * 0.075)
            color: root.darkBackground ? "#d7b94f" : "#f1d56b"
            opacity: root.darkBackground ? 0.46 : 0.70
            rotation: 13
        }
    }
    Rectangle {
        anchors.fill: parent
        color: "#ffffff"
        opacity: root.darkBackground ? 0.015 : 0.07
    }

    TelemetryScene {
        objectName: "visual-smoke-telemetry-scene"
        anchors.fill: parent
        renderContext: root.activeRenderContext
        widgetModel: root.activeWidgetModel
    }

    QtObject {
        id: sampleContext
        property real time: 0
        function telemetryValue(source) {
            const samples = {
                "speed": 50,
                "rpm": 1273,
                "throttle": 32,
                "brake": 0,
                "oil": 97,
                "atf": 84,
                "coolant": 90,
                "heartRate": 105,
                "lateralG": 0.18,
                "longitudinalG": 0.276
            };
            return samples[source];
        }
    }

    Component.onCompleted: {
            const common = {
                "backgroundColor": "#111a22", "backgroundOpacity": 0.86,
                "borderColor": "#8895a3", "borderOpacity": 0.55,
                "cornerRadius": 14, "padding": 14,
                "textColor": "#f2f5f7", "secondaryTextColor": "#b5c0ca"
            };
            const add = function(type, x, y, width, height, settings) {
                smokeWidgets.append({
                    "widgetId": type + "-smoke-" + smokeWidgets.count,
                    "widgetType": type, "widgetX": x, "widgetY": y,
                    "widgetWidth": width, "widgetHeight": height,
                    "widgetScale": 1, "widgetRotation": 0, "widgetOpacity": 1,
                    "widgetVisible": true, "widgetSettings": settings,
                    "widgetCues": [], "widgetGroupId": ""
                });
            };
            add("retroTachometer", 0.025, 0.16, 0.19, 0.36, {
                "showBackground": false, "showBorder": false, "padding": 0,
                "source": "rpm", "label": "RPM", "minValue": 0, "maxValue": 8000,
                "panelColor": "#111a22", "panelOpacity": 0.86, "dialColor": "#f2f5f7",
                "needleColor": "#e14b4b", "warningColor": "#e14b4b", "rimColor": "#8895a3"
            });
            add("speed", 0.22, 0.23, 0.09, 0.24, Object.assign({}, common, {
                "source": "speed", "label": "Speed", "unit": "km/h", "decimals": 0
            }));
            add("pedals", 0.32, 0.23, 0.15, 0.24, Object.assign({}, common, {
                "acceleratorSource": "throttle", "brakeSource": "brake",
                "acceleratorLabel": "Throttle", "brakeLabel": "Brake",
                "acceleratorColor": "#55d76a", "brakeColor": "#e14b4b", "showValues": true
            }));
            add("retroCustomValue", 0.48, 0.23, 0.18, 0.08, Object.assign({}, common, {
                "source": "oil", "label": "OIL", "unit": "°C", "decimals": 0,
                "icon": "◒", "stackPosition": "top"
            }));
            add("retroCustomValue", 0.48, 0.31, 0.18, 0.08, Object.assign({}, common, {
                "source": "atf", "label": "ATF", "unit": "°C", "decimals": 0,
                "icon": "⚙", "stackPosition": "middle"
            }));
            add("retroCustomValue", 0.48, 0.39, 0.18, 0.08, Object.assign({}, common, {
                "source": "coolant", "label": "COOLANT", "unit": "°C", "decimals": 0,
                "icon": "♨", "stackPosition": "bottom"
            }));
            add("heartRate", 0.675, 0.23, 0.10, 0.24, Object.assign({}, common, {
                "source": "heartRate", "label": "HR", "unit": "bpm", "accentColor": "#e14b4b"
            }));
            add("f1GForceRadar", 0.80, 0.08, 0.17, 0.31, {
                "showBackground": false, "showBorder": false, "padding": 0,
                "lateralSource": "lateralG", "longitudinalSource": "longitudinalG",
                "maxG": 1.5, "ringStepG": 0.25, "radarBackgroundColor": "#111a22",
                "backgroundOpacity": 0.86, "dotColor": "#f5a623", "gridColor": "#8895a3"
            });
            add("gForceMagnitudeBar", 0.80, 0.40, 0.17, 0.09, Object.assign({}, common, {
                "lateralSource": "lateralG", "longitudinalSource": "longitudinalG",
                "maxG": 1.5, "labelText": "G-Force", "decimals": 2,
                "barColor": "#f5a623", "barBackgroundColor": "#24303d"
            }));
    }
}
