find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Core Gui Quick QuickControls2 Multimedia Test)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_library(flappedear_core STATIC
    src/gopro/GoProTelemetrySource.cpp
    src/gopro/GoProTelemetrySource.h
    src/sync/TelemetrySyncEngine.cpp
    src/sync/TelemetrySyncEngine.h
    src/telemetry/TelemetrySession.cpp
    src/telemetry/TelemetrySession.h
    src/telemetry/TrackGeometry.cpp
    src/telemetry/TrackGeometry.h
    src/telemetry/VboParser.cpp
    src/telemetry/VboParser.h
    src/widgets/WidgetModel.cpp
    src/widgets/WidgetModel.h
)
target_compile_features(flappedear_core PUBLIC cxx_std_20)
target_include_directories(flappedear_core PUBLIC src)
target_link_libraries(flappedear_core PUBLIC Qt6::Core)
target_compile_options(flappedear_core PRIVATE -Wall -Wextra -Wpedantic)

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
target_compile_options(flappedear_native PRIVATE -Wall -Wextra -Wpedantic)

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
        qml/FeButton.qml
        qml/FeCheckBox.qml
        qml/FeComboBox.qml
        qml/FeSlider.qml
        qml/FeSpinBox.qml
        qml/FeTextField.qml
        qml/InspectorPanel.qml
        qml/Main.qml
        qml/SectionTitle.qml
        qml/WidgetOverlay.qml
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
