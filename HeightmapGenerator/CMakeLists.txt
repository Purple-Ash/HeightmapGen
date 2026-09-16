cmake_minimum_required(VERSION 2.8...4.0)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
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
        library/core/PerlinGenerator.cpp
)

target_include_directories(HeightmapGen
    PUBLIC ${CMAKE_SOURCE_DIR}/library/api
    PRIVATE ${CMAKE_SOURCE_DIR}/library/core
)


add_executable(Tests
    ${TEST_SOURCES}
)

if (NOT DEFINED ENV{CI})
	add_executable(view
			view/view.cpp
	)

	FetchContent_Declare(
			winx
			GIT_REPOSITORY https://github.com/dark-tree/winx
			GIT_TAG        2c4100f986ae3876425e43f1fcfd6fc3eebe0a9d
	)

	FetchContent_Declare(
			glad
			GIT_REPOSITORY https://github.com/Dav1dde/glad
			GIT_TAG        v0.1.36
	)

	set(GLAD_PROFILE "core" CACHE STRING "OpenGL profile")
	set(GLAD_GENERATOR "c" CACHE STRING "Language to generate the binding for")

	FetchContent_MakeAvailable(winx glad)

	target_link_libraries(view PRIVATE
			HeightmapGen
			winx
			glad
	)

	target_include_directories(view PRIVATE
			${winx_SOURCE_DIR}
			${GLAD_INCLUDE_DIRS}
	)
endif()

find_package(Threads REQUIRED)

target_link_libraries(Tests PRIVATE
    HeightmapGen
    GTest::gtest_main
    Threads::Threads
)
