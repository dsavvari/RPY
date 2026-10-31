cmake_minimum_required(VERSION 3.16)

project(rpy_visualizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

set(RPY_SERVER_SOURCES
    src/main.cpp
    src/ReferenceFrame.cpp
    src/Rotation.cpp
    src/ServiceHandler.cpp
)

add_executable(rpy_server ${RPY_SERVER_SOURCES})

target_include_directories(rpy_server PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
)

set(RPY_TIDY_SOURCES ${RPY_SERVER_SOURCES})

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
    list(APPEND RPY_TIDY_SOURCES tests/UnitTests.cpp)

    target_include_directories(rpy_unit_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(rpy_unit_tests PRIVATE GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(rpy_unit_tests)
endif()

find_program(CLANG_TIDY_EXECUTABLE
    NAMES clang-tidy
    HINTS
        "$ENV{HOMEBREW_PREFIX}/opt/llvm/bin"
        /opt/homebrew/opt/llvm/bin
        /usr/local/opt/llvm/bin
)

if(CLANG_TIDY_EXECUTABLE)
    add_custom_target(rpy_lint
        COMMAND "${CLANG_TIDY_EXECUTABLE}"
            -p "${CMAKE_BINARY_DIR}"
            --checks=clang-analyzer-*,bugprone-*,performance-*,modernize-use-emplace,modernize-loop-convert,modernize-make-unique,modernize-use-nullptr,modernize-use-override
            "--header-filter=^${CMAKE_SOURCE_DIR}/(src|tests)/.*"
            ${RPY_TIDY_SOURCES}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Running clang-tidy"
        VERBATIM
    )
else()
    message(STATUS "clang-tidy not found; rpy_lint target is unavailable")
endif()
