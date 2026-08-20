qt_add_executable(flappedear_native_tests
    TelemetryTests.cpp
)
target_compile_features(flappedear_native_tests PRIVATE cxx_std_20)
target_link_libraries(flappedear_native_tests PRIVATE flappedear_core Qt6::Test)
target_compile_options(flappedear_native_tests PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(flappedear_native_tests PRIVATE
    TEST_FIXTURE_PATH="${CMAKE_SOURCE_DIR}/tests/fixtures/basic.vbo"
)
add_test(NAME flappedear_native_tests COMMAND flappedear_native_tests)
