cmake_minimum_required(VERSION 2.8...4.0)
project(HeightmapGen)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

file(GLOB_RECURSE  LIBRARY_SOURCES library/*.cpp)
file(GLOB_RECURSE  TEST_SOURCES tests/*.cpp)


add_library(HeightmapGen SHARED
    ${LIBRARY_SOURCES}
        library/core/Helpers.h
)

target_include_directories(HeightmapGen
    PUBLIC ${CMAKE_SOURCE_DIR}/library/api
    PRIVATE ${CMAKE_SOURCE_DIR}/library/core
)


add_executable(Tests
    ${TEST_SOURCES}
)

find_package(SDL2)

if (SDL2_FOUND)
    add_executable(view
            view/view.cpp
    )

    target_link_libraries(view PRIVATE
            SDL2::SDL2
            HeightmapGen
    )
endif()

find_package(Threads REQUIRED)

target_link_libraries(Tests PRIVATE
    HeightmapGen
    GTest::gtest_main
    Threads::Threads
)
