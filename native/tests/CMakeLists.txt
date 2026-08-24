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
    TelemetryTests.cpp
    ../src/app/AppController.cpp
    ../src/app/AppController.h
)
target_compile_features(flappedear_native_tests PRIVATE cxx_std_20)
target_link_libraries(flappedear_native_tests PRIVATE flappedear_core Qt6::Concurrent Qt6::Gui Qt6::Test)
flappedear_enable_warnings(flappedear_native_tests)
target_compile_definitions(flappedear_native_tests PRIVATE
    TEST_FIXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/basic.vbo"
    TRACK_WIDGET_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../qml/widgets/TrackWidget.qml"
    RAW_TRANSPORT_CONSUMER_PATH="$<TARGET_FILE:flappedear_raw_transport_consumer>"
    PROBE_TEST_HELPER_PATH="$<TARGET_FILE:flappedear_probe_test_helper>"
    FLAPPEDEAR_NATIVE_PATH="$<TARGET_FILE:flappedear_native>"
)
add_dependencies(flappedear_native_tests flappedear_native flappedear_raw_transport_consumer flappedear_probe_test_helper)
add_test(NAME flappedear_native_tests COMMAND flappedear_native_tests)
add_test(NAME flappedear_startup_smoke COMMAND flappedear_native --startup-smoke)
set_tests_properties(flappedear_startup_smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    PASS_REGULAR_EXPRESSION "Startup smoke passed"
    FAIL_REGULAR_EXPRESSION "ReferenceError;TypeError;Binding loop"
)
