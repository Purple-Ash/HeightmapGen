cmake_minimum_required(VERSION 3.20)
project(DatasetGenerator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(DatasetGenerator src/main.cpp)
target_include_directories(DatasetGenerator PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../HeightmapGenerator/library/api"
    "${CMAKE_CURRENT_LIST_DIR}/../HeightmapGenerator/library/core"
)
