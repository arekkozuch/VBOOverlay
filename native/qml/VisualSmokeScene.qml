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
    readonly property var activeRenderContext: root.renderContext || sampleContext
    readonly property var activeWidgetModel: root.widgetModel || smokeWidgets

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#b9dcf0" }
            GradientStop { position: 0.42; color: "#e5f0e6" }
            GradientStop { position: 0.421; color: "#4b5a58" }
            GradientStop { position: 1.0; color: "#1c252c" }
        }
    }
    Rectangle {
        width: parent.width * 0.55
        height: parent.height * 1.25
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.36
        rotation: 13
        transformOrigin: Item.Top
        color: "#303d43"
        opacity: 0.84
    }
    Repeater {
        model: 7
        Rectangle {
            required property int index
            width: root.width * (0.012 + index * 0.006)
            height: root.height * (0.055 + index * 0.015)
            x: root.width * 0.49 + index * root.width * 0.008
            y: root.height * (0.46 + index * 0.075)
            color: "#f1d56b"
            opacity: 0.70
            rotation: 13
        }
    }
    Rectangle {
        anchors.fill: parent
        color: "#ffffff"
        opacity: 0.07
    }

    TelemetryScene {
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
                "rpm": 6240,
                "throttle": 32,
                "brake": 0,
                "oil": 97,
                "atf": 84,
                "coolant": 90,
                "heartRate": 105,
                "lateralG": 0.24,
                "longitudinalG": 0.22
            };
            return samples[source];
        }
    }

    ListModel {
        id: smokeWidgets
        Component.onCompleted: {
            const common = {
                "backgroundColor": "#101820", "backgroundOpacity": 0.86,
                "borderColor": "#718397", "borderOpacity": 0.55,
                "cornerRadius": 12, "padding": 14,
                "textColor": "#f2f5f7", "secondaryTextColor": "#c0c8d0"
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
            add("retroTachometer", 0.045, 0.49, 0.25, 0.36, {
                "showBackground": false, "showBorder": false, "padding": 0,
                "source": "rpm", "label": "RPM", "minValue": 0, "maxValue": 8000,
                "panelColor": "#101820", "panelOpacity": 0.86, "dialColor": "#f2f5f7",
                "needleColor": "#e14b4b", "warningColor": "#e14b4b", "rimColor": "#718397"
            });
            add("speed", 0.055, 0.07, 0.16, 0.19, Object.assign({}, common, {
                "source": "speed", "label": "Speed", "unit": "km/h", "decimals": 0
            }));
            add("pedals", 0.72, 0.06, 0.23, 0.17, Object.assign({}, common, {
                "acceleratorSource": "throttle", "brakeSource": "brake",
                "acceleratorLabel": "Throttle", "brakeLabel": "Brake",
                "acceleratorColor": "#55d76a", "brakeColor": "#e14b4b", "showValues": true
            }));
            add("retroCustomValue", 0.73, 0.28, 0.22, 0.055, {
                "showBackground": false, "showBorder": false, "padding": 0,
                "source": "oil", "label": "OIL", "unit": "°C", "decimals": 0
            });
            add("retroCustomValue", 0.73, 0.335, 0.22, 0.055, {
                "showBackground": false, "showBorder": false, "padding": 0,
                "source": "atf", "label": "ATF", "unit": "°C", "decimals": 0
            });
            add("retroCustomValue", 0.73, 0.39, 0.22, 0.055, {
                "showBackground": false, "showBorder": false, "padding": 0,
                "source": "coolant", "label": "COOLANT", "unit": "°C", "decimals": 0
            });
            add("heartRate", 0.39, 0.07, 0.13, 0.20, Object.assign({}, common, {
                "source": "heartRate", "label": "HR", "unit": "bpm", "accentColor": "#e14b4b"
            }));
            add("f1GForceRadar", 0.72, 0.52, 0.20, 0.30, {
                "showBackground": false, "showBorder": false, "padding": 0,
                "lateralSource": "lateralG", "longitudinalSource": "longitudinalG",
                "maxG": 1.5, "ringStepG": 0.25, "radarBackgroundColor": "#101820",
                "backgroundOpacity": 0.86, "dotColor": "#f5a623", "gridColor": "#91a1b1"
            });
            add("gForceMagnitudeBar", 0.36, 0.78, 0.28, 0.10, Object.assign({}, common, {
                "lateralSource": "lateralG", "longitudinalSource": "longitudinalG",
                "maxG": 1.5, "labelText": "G-Force", "decimals": 2,
                "barColor": "#f5a623", "barBackgroundColor": "#24303d"
            }));
        }
    }
}
