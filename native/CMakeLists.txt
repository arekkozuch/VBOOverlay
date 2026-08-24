find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Core Gui GuiPrivate Qml Quick QuickControls2 Multimedia Test)

qt_standard_project_setup(REQUIRES 6.8)

function(flappedear_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

qt_add_library(flappedear_core STATIC
    src/app/AppLog.cpp
    src/app/AppLog.h
    src/export/EncoderDetector.cpp
    src/export/EncoderDetector.h
    src/export/ExportDiagnostics.cpp
    src/export/ExportDiagnostics.h
    src/export/ExportArtifactManifest.cpp
    src/export/ExportArtifactManifest.h
    src/export/ExportEngine.cpp
    src/export/ExportMediaProfile.cpp
    src/export/ExportMediaProfile.h
    src/export/ExportFormat.cpp
    src/export/ExportEngine.h
    src/export/ExportOutputTransaction.cpp
    src/export/ExportOutputTransaction.h
    src/export/ExportProgress.cpp
    src/export/ExportProgress.h
    src/export/ExportProcessSupervisor.cpp
    src/export/ExportProcessSupervisor.h
    src/export/ExportStoragePolicy.cpp
    src/export/ExportStoragePolicy.h
    src/export/ExportTargetIdentity.cpp
    src/export/ExportTargetIdentity.h
    src/export/FfmpegTools.cpp
    src/export/FfmpegTools.h
    src/export/MediaProbe.cpp
    src/export/MediaProbe.h
    src/export/PersistentExportLog.cpp
    src/export/PersistentExportLog.h
    src/export/RawFrameTransport.cpp
    src/export/RawFrameTransport.h
    src/export/TelemetryFrameRenderer.cpp
    src/export/TelemetryFrameRenderer.h
    src/export/TemporaryOverlayValidation.cpp
    src/export/TemporaryOverlayValidation.h
    src/gopro/GoProTelemetrySource.cpp
    src/gopro/GoProTelemetrySource.h
    src/project/ProjectWriter.cpp
    src/project/ProjectWriter.h
    src/project/ProjectDocumentState.cpp
    src/project/ProjectDocumentState.h
    src/project/ProjectRecoveryStore.cpp
    src/project/ProjectRecoveryStore.h
    src/project/ProjectSourceReference.cpp
    src/project/ProjectSourceReference.h
    src/sync/TelemetrySyncEngine.cpp
    src/sync/TelemetrySyncEngine.h
    src/telemetry/TelemetrySession.cpp
    src/telemetry/TelemetrySession.h
    src/telemetry/SourceOperation.h
    src/telemetry/TelemetryRenderContext.cpp
    src/telemetry/TelemetryRenderContext.h
    src/telemetry/TrackGeometry.cpp
    src/telemetry/TrackGeometry.h
    src/telemetry/VboParser.cpp
    src/telemetry/VboParser.h
    src/widgets/WidgetModel.cpp
    src/widgets/WidgetModel.h
)
target_compile_features(flappedear_core PUBLIC cxx_std_20)
target_include_directories(flappedear_core PUBLIC src)
target_link_libraries(flappedear_core PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
# QRhi is a Qt Gui private API. Keep it implementation-only: no public headers
# in flappedear_core expose QRhi types.
target_link_libraries(flappedear_core PRIVATE Qt6::GuiPrivate)
flappedear_enable_warnings(flappedear_core)

qt_add_resources(flappedear_core "flappedear_templates"
    PREFIX "/flappedear"
    FILES
        resources/widget-templates.json
)

set(FLAPPEDEAR_PLATFORM_ICON)
if(APPLE)
    set(FLAPPEDEAR_APP_ICON resources/branding/FlappedEar.icns)
    set_source_files_properties(${FLAPPEDEAR_APP_ICON} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    list(APPEND FLAPPEDEAR_PLATFORM_ICON ${FLAPPEDEAR_APP_ICON})
elseif(WIN32)
    list(APPEND FLAPPEDEAR_PLATFORM_ICON resources/branding/app-icon.rc)
endif()

qt_add_executable(flappedear_native MACOSX_BUNDLE
    src/app/AppController.cpp
    src/app/AppController.h
    src/main.cpp
    ${FLAPPEDEAR_PLATFORM_ICON}
)
target_compile_features(flappedear_native PRIVATE cxx_std_20)
target_link_libraries(flappedear_native PRIVATE
    flappedear_core
    Qt6::Core
    Qt6::Concurrent
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
)
flappedear_enable_warnings(flappedear_native)

qt_add_resources(flappedear_native "flappedear_branding"
    PREFIX "/flappedear"
    FILES
        resources/branding/app-logo.png
)

qt_add_qml_module(flappedear_native
    URI FlappedEar
    VERSION 1.0
    QML_FILES
        qml/ColorField.qml
        qml/AnalysisPanel.qml
        qml/AnalysisWindow.qml
        qml/FeButton.qml
        qml/FeCheckBox.qml
        qml/FeComboBox.qml
        qml/FeSlider.qml
        qml/FeSpinBox.qml
        qml/FeTextField.qml
        qml/InspectorPanel.qml
        qml/Main.qml
        qml/SectionTitle.qml
        qml/TrackMapPanel.qml
        qml/TelemetryScene.qml
        qml/VisualSmokeScene.qml
        qml/WidgetOverlay.qml
        qml/widgets/ArcGaugeWidget.qml
        qml/widgets/BrandLogoWidget.qml
        qml/widgets/CustomValueWidget.qml
        qml/widgets/DialGaugeWidget.qml
        qml/widgets/GForceWidget.qml
        qml/widgets/GForceData.qml
        qml/widgets/F1GForceRadarWidget.qml
        qml/widgets/GForceMagnitudeBarWidget.qml
        qml/widgets/HeartRateWidget.qml
        qml/widgets/PedalsWidget.qml
        qml/widgets/RetroGearWidget.qml
        qml/widgets/RetroCustomValueWidget.qml
        qml/widgets/RetroGrandPrixWidget.qml
        qml/widgets/RetroNameplateWidget.qml
        qml/widgets/RetroPedalWidget.qml
        qml/widgets/RetroSpeedArcWidget.qml
        qml/widgets/RetroTachometerWidget.qml
        qml/widgets/RpmWidget.qml
        qml/widgets/SpeedWidget.qml
        qml/widgets/TelemetryOverlayWidget.qml
        qml/widgets/TrackWidget.qml
)

set_target_properties(flappedear_native PROPERTIES
    OUTPUT_NAME "FlappedEar Telemetry"
    MACOSX_BUNDLE_BUNDLE_NAME "FlappedEar Telemetry"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.flappedear.telemetry"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.2.0"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
    MACOSX_BUNDLE_ICON_FILE "FlappedEar.icns"
    WIN32_EXECUTABLE TRUE
)

install(TARGETS flappedear_native
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)

add_subdirectory(tests)
