cmake_minimum_required(VERSION 3.24)

project(FlappedEarTelemetry LANGUAGES CXX)

if(WIN32)
    enable_language(RC)
endif()

enable_testing()
add_subdirectory(native)
