qt_add_executable(flappedear_raw_transport_consumer
    RawTransportConsumer.cpp
)
target_compile_features(flappedear_raw_transport_consumer PRIVATE cxx_std_20)
target_link_libraries(flappedear_raw_transport_consumer PRIVATE Qt6::Core)
flappedear_enable_warnings(flappedear_raw_transport_consumer)

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
    RAW_TRANSPORT_CONSUMER_PATH="$<TARGET_FILE:flappedear_raw_transport_consumer>"
)
add_dependencies(flappedear_native_tests flappedear_raw_transport_consumer)
add_test(NAME flappedear_native_tests COMMAND flappedear_native_tests)
