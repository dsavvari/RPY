cmake_minimum_required(VERSION 3.16)

project(rpy_visualizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

add_executable(rpy_server
    src/main.cpp
    src/ReferenceFrame.cpp
    src/Rotation.cpp
    src/ServiceHandler.cpp
)

target_include_directories(rpy_server PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
)

if(BUILD_TESTING)
    find_package(GTest QUIET)

    if(NOT GTest_FOUND)
        include(FetchContent)

        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(rpy_unit_tests
        tests/UnitTests.cpp
        src/ReferenceFrame.cpp
        src/Rotation.cpp
    )

    target_include_directories(rpy_unit_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(rpy_unit_tests PRIVATE GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(rpy_unit_tests)
endif()
