cmake_minimum_required(VERSION 3.20)
project(VoptixDataGenerator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT WIN32)
    message(FATAL_ERROR "VoptixDataGenerator currently targets the Windows HeightmapGen DLL.")
endif()

add_executable(VoptixDataGenerator src/main.cpp)
target_include_directories(VoptixDataGenerator PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../HeightmapGenerator/library/api"
    "${CMAKE_CURRENT_LIST_DIR}/../HeightmapGenerator/library/core")
