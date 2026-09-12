qt_add_executable(flappedear_gui_session_lock_helper
    GuiSessionLockHelper.cpp
    ../src/app/GuiSessionLock.cpp
)
target_compile_features(flappedear_gui_session_lock_helper PRIVATE cxx_std_20)
target_include_directories(flappedear_gui_session_lock_helper PRIVATE ../src)
target_link_libraries(flappedear_gui_session_lock_helper PRIVATE Qt6::Core)
flappedear_enable_warnings(flappedear_gui_session_lock_helper)

qt_add_executable(flappedear_raw_transport_consumer
    RawTransportConsumer.cpp
)
target_compile_features(flappedear_raw_transport_consumer PRIVATE cxx_std_20)
target_link_libraries(flappedear_raw_transport_consumer PRIVATE Qt6::Core)
flappedear_enable_warnings(flappedear_raw_transport_consumer)

qt_add_executable(flappedear_probe_test_helper ProbeTestHelper.cpp)
target_compile_features(flappedear_probe_test_helper PRIVATE cxx_std_20)
target_link_libraries(flappedear_probe_test_helper PRIVATE Qt6::Core)
flappedear_enable_warnings(flappedear_probe_test_helper)

qt_add_executable(flappedear_native_tests
    ../src/app/AppControllerImport.cpp
    ../src/app/AppControllerOuting.cpp
    TelemetryTests.cpp
    ../src/app/AppController.cpp
    ../src/app/AppController.h
)
target_compile_features(flappedear_native_tests PRIVATE cxx_std_20)
target_link_libraries(flappedear_native_tests PRIVATE flappedear_core Qt6::Concurrent Qt6::Gui Qt6::Multimedia Qt6::QuickControls2 Qt6::Test ZLIB::ZLIBSTATIC)
flappedear_enable_warnings(flappedear_native_tests)
qt_add_resources(flappedear_native_tests "flappedear_test_qml"
    PREFIX "/qt/qml/FlappedEar/qml"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/../qml"
    FILES
        ../qml/TelemetryScene.qml
        ../qml/widgets/ArcGaugeWidget.qml
        ../qml/widgets/ComparisonTileWidget.qml
        ../qml/widgets/DialGaugeWidget.qml
        ../qml/widgets/RetroGrandPrixWidget.qml
        ../qml/widgets/RetroSpeedArcWidget.qml
        ../qml/widgets/RetroTachometerWidget.qml
        ../qml/widgets/TelemetryPanel.qml
)
target_compile_definitions(flappedear_native_tests PRIVATE
    GUI_SESSION_LOCK_HELPER_PATH="$<TARGET_FILE:flappedear_gui_session_lock_helper>"
    TEST_FIXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/basic.vbo"
    ANALYSIS_PANEL_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../qml/AnalysisPanel.qml"
    BATCH_IMPORT_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../qml/BatchImportDialog.qml"
    TRACK_WIDGET_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../qml/widgets/TrackWidget.qml"
    RAW_TRANSPORT_CONSUMER_PATH="$<TARGET_FILE:flappedear_raw_transport_consumer>"
    PROBE_TEST_HELPER_PATH="$<TARGET_FILE:flappedear_probe_test_helper>"
    FLAPPEDEAR_NATIVE_PATH="$<TARGET_FILE:flappedear_native>"
)
add_dependencies(flappedear_native_tests flappedear_gui_session_lock_helper flappedear_native flappedear_raw_transport_consumer flappedear_probe_test_helper)
add_test(NAME flappedear_native_tests COMMAND flappedear_native_tests)
# Window interaction tests need real native handles for the QRhi swapchain.
# Offscreen QPA supplies synthetic IDs, not NSView/HWND handles. Preserve the
# renderer/backend and all export pixel assertions; startup smoke is separate.
if(APPLE)
    set_tests_properties(flappedear_native_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=cocoa")
elseif(WIN32)
    set_tests_properties(flappedear_native_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=windows")
endif()
qt_add_executable(flappedear_rcz_tests RczTests.cpp RczFixture.h)
target_compile_features(flappedear_rcz_tests PRIVATE cxx_std_20)
target_link_libraries(flappedear_rcz_tests PRIVATE flappedear_core Qt6::Test ZLIB::ZLIBSTATIC)
flappedear_enable_warnings(flappedear_rcz_tests)
add_test(NAME flappedear_rcz_tests COMMAND flappedear_rcz_tests)
qt_add_executable(flappedear_import_tests TelemetryImportTests.cpp RczFixture.h)
target_compile_features(flappedear_import_tests PRIVATE cxx_std_20)
target_link_libraries(flappedear_import_tests PRIVATE flappedear_core Qt6::Test ZLIB::ZLIBSTATIC)
flappedear_enable_warnings(flappedear_import_tests)
add_test(NAME flappedear_import_tests COMMAND flappedear_import_tests)
qt_add_executable(flappedear_event_project_tests EventProjectTests.cpp EventProjectFixture.h)
target_compile_features(flappedear_event_project_tests PRIVATE cxx_std_20)
target_link_libraries(flappedear_event_project_tests PRIVATE flappedear_core Qt6::Test)
target_compile_definitions(flappedear_event_project_tests PRIVATE
    EVENT_PROJECT_FIXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/event-demo.fetproject"
)
flappedear_enable_warnings(flappedear_event_project_tests)
add_test(NAME flappedear_event_project_tests COMMAND flappedear_event_project_tests)
add_test(NAME flappedear_startup_smoke COMMAND flappedear_native --startup-smoke)
set_tests_properties(flappedear_startup_smoke PROPERTIES
    # Startup checks QML loading and controller/window lifecycle. Offscreen QPA
    # cannot supply a native window swapchain; QRhi pixels are tested separately.
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    PASS_REGULAR_EXPRESSION "Startup smoke passed"
    FAIL_REGULAR_EXPRESSION "ReferenceError;TypeError;Binding loop"
)
