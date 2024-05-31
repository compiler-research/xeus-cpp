#############################################################################
# Copyright (c) 2023, xeus-cpp contributors                                 #
#                                                                           #
# Distributed under the terms of the BSD 3-Clause License.                  #
#                                                                           #
# The full license is in the file LICENSE, distributed with this software.  #
#############################################################################


# Unit tests
# ==========

cmake_minimum_required(VERSION 3.24)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(xeus-cpp-test)

    enable_testing()

    find_package(xeus-cpp REQUIRED CONFIG)
endif()

include(CheckCXXCompilerFlag)

string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

if(CMAKE_CXX_COMPILER_ID MATCHES Clang OR CMAKE_CXX_COMPILER_ID MATCHES GNU OR CMAKE_CXX_COMPILER_ID MATCHES Intel)
    add_compile_options(-Wunused-parameter -Wextra -Wreorder -Wconversion -Wsign-conversion)

    CHECK_CXX_COMPILER_FLAG(-march=native HAS_MARCH_NATIVE)
    if (HAS_MARCH_NATIVE)
        add_compile_options(-march=native)
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
    add_compile_options(/EHsc /MP /bigobj)
    add_link_options(/MANIFEST:NO)
endif()

find_package(doctest)
find_package(Threads)

set(XEUS_CPP_TESTS
    main.cpp
    test_interpreter.cpp
)

add_executable(test_xeus_cpp ${XEUS_CPP_TESTS})

if (APPLE)
    set_target_properties(test_xeus_cpp PROPERTIES
        MACOSX_RPATH ON
    )
else()
    set_target_properties(test_xeus_cpp PROPERTIES
        BUILD_WITH_INSTALL_RPATH 1
        SKIP_BUILD_RPATH FALSE
    )
endif()

set_target_properties(test_xeus_cpp PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

target_link_libraries(test_xeus_cpp xeus-cpp doctest::doctest ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(test_xeus_cpp PRIVATE ${XEUS_CPP_INCLUDE_DIR})

add_custom_target(xtest COMMAND test_xeus_cpp DEPENDS test_xeus_cpp)
