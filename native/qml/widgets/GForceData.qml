import QtQuick

QtObject {
    // Shared presentation-only G-force mapping.  It deliberately reads the raw
    // channels independently of the telemetry model, so an absent axis stays absent.
    required property var frame

    readonly property var lateralRaw: frame.raw("lateralSource", "lateralAcceleration")
    readonly property var longitudinalRaw: frame.raw("longitudinalSource", "longitudinalAcceleration")
    readonly property bool hasValue: lateralRaw !== undefined && lateralRaw !== null
        && longitudinalRaw !== undefined && longitudinalRaw !== null
        && Number.isFinite(Number(lateralRaw)) && Number.isFinite(Number(longitudinalRaw))
    readonly property real lateral: hasValue
        ? Number(lateralRaw) * ((frame.widgetSettings.invertLateral ?? false) ? -1 : 1) : 0
    readonly property real longitudinal: hasValue
        ? Number(longitudinalRaw) * ((frame.widgetSettings.invertLongitudinal ?? false) ? -1 : 1) : 0
    // Sign inversions do not affect magnitude, but use displayed values so all G widgets
    // share exactly the same presentation inputs.
    readonly property real combinedG: hasValue ? Math.sqrt(lateral * lateral + longitudinal * longitudinal) : 0
}
