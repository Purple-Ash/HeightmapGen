cmake_minimum_required(VERSION 2.8...4.0)
project(HeightmapGen)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

# ------------------------------------------------------------
# GoogleTest
# ------------------------------------------------------------

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ------------------------------------------------------------
# ONNX Runtime
# ------------------------------------------------------------

if(WIN32)
    set(ONNXRUNTIME_ROOT "../third-party/onnxruntime")
else()
    set(ONNXRUNTIME_ROOT "/opt/onnxruntime")
endif()

add_library(onnxruntime SHARED IMPORTED)

set_target_properties(onnxruntime PROPERTIES
    IMPORTED_LOCATION
        "${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so"

    INTERFACE_INCLUDE_DIRECTORIES
        "${ONNXRUNTIME_ROOT}/include"

    BUILD_RPATH
        "${ONNXRUNTIME_ROOT}/lib"
)

# ------------------------------------------------------------
# HeightmapGen
# ------------------------------------------------------------

file(GLOB_RECURSE  LIBRARY_SOURCES library/*.cpp)

add_library(HeightmapGen SHARED
    ${LIBRARY_SOURCES}
        library/core/Helpers.h
        library/core/PerlinGenerator.cpp
)

target_include_directories(HeightmapGen
    PUBLIC ${CMAKE_SOURCE_DIR}/library/api
    PRIVATE ${CMAKE_SOURCE_DIR}/library/core
)

target_link_libraries(HeightmapGen
    PUBLIC onnxruntime
)

# ------------------------------------------------------------
# Tests
# ------------------------------------------------------------

file(GLOB_RECURSE  TEST_SOURCES tests/*.cpp)

add_executable(Tests
    ${TEST_SOURCES}
)

find_package(Threads REQUIRED)

target_link_libraries(Tests PRIVATE
    HeightmapGen
    GTest::gtest_main
    Threads::Threads
)
