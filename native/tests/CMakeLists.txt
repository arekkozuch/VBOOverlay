qt_add_executable(flappedear_native_tests
    TelemetryTests.cpp
)
target_compile_features(flappedear_native_tests PRIVATE cxx_std_20)
target_link_libraries(flappedear_native_tests PRIVATE flappedear_core Qt6::Test)
flappedear_enable_warnings(flappedear_native_tests)
target_compile_definitions(flappedear_native_tests PRIVATE
    TEST_FIXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/basic.vbo"
)
add_test(NAME flappedear_native_tests COMMAND flappedear_native_tests)
