cmake_minimum_required(VERSION 3.16)

project(rpy_visualizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(rpy_server
    src/main.cpp
    src/Rotation.cpp
    src/ServiceHandler.cpp
)

target_include_directories(rpy_server PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
)
