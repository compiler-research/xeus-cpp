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

    if(NOT EMSCRIPTEN)
        add_compile_options(-Wunused-parameter -Wextra -Wreorder -Wconversion -Wsign-conversion)
    endif()

endif()

if(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
    add_compile_options(/EHsc /MP /bigobj)
    add_link_options(/MANIFEST:NO)
endif()

find_package(doctest)

set(XEUS_CPP_TESTS
    main.cpp
    test_interpreter.cpp
)

add_executable(test_xeus_cpp ${XEUS_CPP_TESTS})

if(EMSCRIPTEN)
    target_link_libraries(test_xeus_cpp PRIVATE xeus-cpp-static doctest::doctest)

    target_compile_options(test_xeus_cpp
        PUBLIC "SHELL: ${XEUS_CPP_WASM_KERNEL_EXTRA_FLAGS}"
    )
    # So we create a html file, as well as the javascript file
    set_target_properties(test_xeus_cpp PROPERTIES
        SUFFIX ".html"
    )
    # --emrun
    # Makes it so that we run the html file created by this target, that we can capture the
    # standard output and output it to the terminal
    # MINIFY_HTML=0
    # Without this MINIFY_HTML=0 flag you end up with the situation where the creation of the
    # test_xeus_cpp.html file breaks on MacOS, but not Ubuntu
    target_link_options(test_xeus_cpp
        PUBLIC "SHELL: ${XEUS_CPP_WASM_KERNEL_EXTRA_FLAGS}"
        PUBLIC "SHELL: -s MAIN_MODULE=1"
        PUBLIC "SHELL: -s WASM_BIGINT"
        PUBLIC "SHELL: -s ASSERTIONS=0"
        PUBLIC "SHELL: -s ALLOW_MEMORY_GROWTH=1"
        PUBLIC "SHELL: -s STACK_SIZE=32mb"
        PUBLIC "SHELL: -s INITIAL_MEMORY=64MB"
        PUBLIC "SHELL: --preload-file ${ESCAPED_SYSROOT_PATH}/include@/include"
        PUBLIC "SHELL: --preload-file ../${XEUS_CPP_DATA_DIR}@/share/xeus-cpp"
        PUBLIC "SHELL: --preload-file ../${XEUS_CPP_CONF_DIR}@/etc/xeus-cpp"
        PUBLIC "SHELL: --emrun"
        PUBLIC "SHELL: -s MINIFY_HTML=0"
    )

    target_include_directories(test_xeus_cpp PRIVATE ${XEUS_CPP_INCLUDE_DIR})

    add_custom_command(TARGET test_xeus_cpp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_INSTALL_PREFIX}/lib/libclangCppInterOp.so
        ${CMAKE_CURRENT_BINARY_DIR}/libclangCppInterOp.so
        COMMENT "Copying libclangCppInterOp.so to the test directory"
    )

    add_custom_command(TARGET test_xeus_cpp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_INSTALL_PREFIX}/lib/libxeus.so
        ${CMAKE_CURRENT_BINARY_DIR}/libxeus.so
        COMMENT "Copying libxeus.so to the test directory"
    )
    add_custom_target(check-xeus-cpp
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR} node test_xeus_cpp.js
        DEPENDS test_xeus_cpp
    )
else()
    find_package(Threads)

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

    add_custom_target(check-xeus-cpp COMMAND test_xeus_cpp DEPENDS test_xeus_cpp)
endif()
