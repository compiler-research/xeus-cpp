#############################################################################
# Copyright (c) 2023, xeus-cpp contributors                                 #
#                                                                           #
# Distributed under the terms of the BSD 3-Clause License.                  #
#                                                                           #
# The full license is in the file LICENSE, distributed with this software.  #
#############################################################################

cmake_minimum_required(VERSION 3.24)
project(xeus-cpp)

enable_language(CXX)
set(CMAKE_CXX_EXTENSIONS NO)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

set(XEUS_CPP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# xeus-cpp tag files
set(XEUS_CPP_DATA_DIR "share/xeus-cpp")
set(XCPP_TAGFILES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/share/xeus-cpp/tagfiles)

set(XEUS_CPP_CONF_DIR "etc/xeus-cpp")
set(XCPP_TAGCONFS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/etc/xeus-cpp/tags.d)

file(COPY "${XCPP_TAGFILES_DIR}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/share/xeus-cpp")
file(COPY "${XCPP_TAGCONFS_DIR}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/etc/xeus-cpp")

# Versioning
# ===========

file(STRINGS "${XEUS_CPP_INCLUDE_DIR}/xeus-cpp/xeus_cpp_config.hpp" version_defines
     REGEX "#define XEUS_CPP_VERSION_(MAJOR|MINOR|PATCH|LABEL)")
foreach (ver ${version_defines})
    if (ver MATCHES "#define XEUS_CPP_VERSION_(MAJOR|MINOR|PATCH|LABEL) +([^ ]+)$")
        set(XEUS_CPP_VERSION_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE INTERNAL "")
    endif ()
endforeach ()
set(${PROJECT_NAME}_VERSION
    ${XEUS_CPP_VERSION_MAJOR}.${XEUS_CPP_VERSION_MINOR}.${XEUS_CPP_VERSION_PATCH}${XEUS_CPP_VERSION_LABEL})
message(STATUS "Building xeus-cpp v${${PROJECT_NAME}_VERSION}")

# Build options
# =============

option(XEUS_CPP_BUILD_STATIC "Build xeus-cpp static library" ON)
option(XEUS_CPP_BUILD_SHARED "Split xcpp build into executable and library" ON)
option(XEUS_CPP_BUILD_EXECUTABLE "Build the xcpp executable" ON)

option(XEUS_CPP_USE_SHARED_XEUS "Link xcpp with the xeus shared library (instead of the static library)" ON)
option(XEUS_CPP_USE_SHARED_XEUS_CPP "Link xcpp with the xeus-cpp shared library (instead of the static library)" ON)
option(XEUS_CPP_EMSCRIPTEN_WASM_BUILD "Build for wasm with emscripten" OFF)

# Test options
option(XEUS_CPP_BUILD_TESTS "xeus-cpp test suite" ON)
option(XEUS_CPP_ENABLE_CODE_COVERAGE "xeus-cpp test suite" OFF)
if(XEUS_CPP_ENABLE_CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  string(TOUPPER "${CMAKE_BUILD_TYPE}" uppercase_CMAKE_BUILD_TYPE)
  if(NOT uppercase_CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    message(FATAL_ERROR "CodeCov enabled on non-debug build!")
  endif()
  set(GCC_COVERAGE_COMPILE_FLAGS "-fprofile-arcs -ftest-coverage")
  set(GCC_COVERAGE_LINK_FLAGS    "--coverage")
  set(CMAKE_CXX_FLAGS            "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS     "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_SHAREDLINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")
  set(CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")
endif()

if(EMSCRIPTEN)
    add_compile_definitions(XEUS_CPP_EMSCRIPTEN_WASM_BUILD)
    message("Build with emscripten")
    set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)
    set(XEUS_CPP_BUILD_SHARED OFF)
    set(XEUS_CPP_BUILD_EXECUTABLE OFF)
    set(XEUS_CPP_USE_SHARED_XEUS_CPP OFF)
    # ENV (https://github.com/emscripten-core/emscripten/commit/6d9681ad04f60b41ef6345ab06c29bbc9eeb84e0)
    set(EMSCRIPTEN_FEATURES "${EMSCRIPTEN_FEATURES} -s \"EXPORTED_RUNTIME_METHODS=[ENV']\"")
endif()

# Dependencies
# ============

set(xeus_REQUIRED_VERSION 5.0.0)
set(xeus_zmq_REQUIRED_VERSION 3.0.0)
set(CppInterOp_REQUIRED_VERSION 1.7.0)
set(xeus_lite_REQUIRED_VERSION 3.2.1)

if (NOT TARGET xeus AND NOT TARGET xeus-static)
    find_package(xeus ${xeus_REQUIRED_VERSION} REQUIRED)
    if (xeus_FOUND)
        message(STATUS "Found compatible xeus version: ${xeus_VERSION}")
    endif()
endif ()

find_package(CppInterOp ${CppInterOp_REQUIRED_VERSION} REQUIRED)
if(CppInterOp_FOUND)
  message(STATUS "Found CppInterOp: config=${CPPINTEROP_CMAKE_DIR} dir=${CPPINTEROP_INSTALL_PREFIX} (found version=${CPPINTEROP_VERSION} compatible with Clang ${CPPINTEROP_LLVM_VERSION_MAJOR}.x)")
endif()

find_package(argparse REQUIRED)
find_package(pugixml REQUIRED)

# Configuration
# =============

if(NOT DEFINED XEUS_CPP_RESOURCE_DIR)
    set(XEUS_CPP_RESOURCE_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/clang/${CPPINTEROP_LLVM_VERSION_MAJOR}")
    message(STATUS "Using clang resource dir headers from ${XEUS_CPP_RESOURCE_DIR}")
endif()

if (NOT DEFINED XEUS_CPP_KERNELSPEC_PATH)
    set(XEUS_CPP_KERNELSPEC_PATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/")
endif ()

function(configure_kernel kernel)
  if(EMSCRIPTEN)
    set(prefix "wasm_")
  else()
    set(XEUS_CPP_PATH "$ENV{PATH}")
    set(XEUS_CPP_LD_LIBRARY_PATH "$ENV{LD_LIBRARY_PATH}")
    set(XEUS_CPP_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include)
  endif()
  if (WIN32)
    string(REPLACE "\\" "/" kernel "${kernel}")
    string(REPLACE "\\" "/" XEUS_CPP_PATH "${XEUS_CPP_PATH}")
    string(REPLACE "\\" "/" XEUS_CPP_LD_LIBRARY_PATH "${XEUS_CPP_LD_LIBRARY_PATH}")
    string(REPLACE "\\" "/" XEUS_CPP_RESOURCE_DIR "${XEUS_CPP_RESOURCE_DIR}")
    string(REPLACE "\\" "/" XEUS_CPP_INCLUDE_DIR "${XEUS_CPP_INCLUDE_DIR}")
  endif()
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}${kernel}${prefix}kernel.json.in"
    "${CMAKE_CURRENT_BINARY_DIR}${kernel}kernel.json"
  )
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}${kernel}logo-32x32.png"
    "${CMAKE_CURRENT_BINARY_DIR}${kernel}"
    COPYONLY
  )
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}${kernel}logo-64x64.png"
    "${CMAKE_CURRENT_BINARY_DIR}${kernel}"
    COPYONLY
  )
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}${kernel}logo-svg.svg"
    "${CMAKE_CURRENT_BINARY_DIR}${kernel}"
    COPYONLY
  )
endfunction()

message("Configure kernels: ...")
configure_kernel("/share/jupyter/kernels/xcpp17/")
configure_kernel("/share/jupyter/kernels/xcpp20/")
configure_kernel("/share/jupyter/kernels/xcpp23/")

# Source files
# ============

set(XEUS_CPP_HEADERS
    include/xeus-cpp/xbuffer.hpp
    include/xeus-cpp/xholder.hpp
    include/xeus-cpp/xoptions.hpp
    include/xeus-cpp/xeus_cpp_config.hpp
    include/xeus-cpp/xholder.hpp
    include/xeus-cpp/xinterpreter.hpp
    include/xeus-cpp/xmanager.hpp
    include/xeus-cpp/xmagics.hpp
    include/xeus-cpp/xoptions.hpp
    include/xeus-cpp/xpreamble.hpp
    #src/xinspect.hpp
    #src/xsystem.hpp
    #src/xparser.hpp
)

set(XEUS_CPP_SRC
    src/xholder.cpp
    src/xinput.cpp
    src/xinspect.cpp
    src/xinterpreter.cpp
    src/xoptions.cpp
    src/xparser.cpp
    src/xutils.cpp
    src/xmagics/os.cpp
)

if(NOT EMSCRIPTEN)
    list(APPEND XEUS_CPP_SRC
        src/xmagics/xassist.cpp
    )
endif()

set(XEUS_CPP_MAIN_SRC
    src/main.cpp
)

# Targets and link - Macros
# =========================

include(CheckCXXCompilerFlag)

string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
list(REMOVE_DUPLICATES CMAKE_INSTALL_RPATH)

macro(xeus_cpp_set_common_options target_name)
    if (MSVC)
        target_compile_options(${target_name} PUBLIC /wd4251 /wd4141)
        target_compile_options(${target_name} PUBLIC /wd4018 /wd4267 /wd4715 /wd4146 /wd4129)
        target_compile_options(${target_name} PUBLIC /EHsc)
    else ()
        target_compile_options(${target_name} PUBLIC -fexceptions)
    endif ()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
        CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR
        CMAKE_CXX_COMPILER_ID MATCHES "Intel")

        if (EMSCRIPTEN)
            target_compile_options(${target_name} PRIVATE -fPIC)
        else ()
            target_compile_options(${target_name} PUBLIC -Wunused-parameter -Wextra -Wreorder)
        endif()

        # C++17 flag
        CHECK_CXX_COMPILER_FLAG("-std=c++17" HAS_CPP_17_FLAG)
        if (HAS_CPP_17_FLAG)
            target_compile_features(${target_name} PRIVATE cxx_std_17)
        else ()
            message(FATAL_ERROR "Unsupported compiler -- xeus-cpp requires C++17 support!")
        endif ()
    endif ()

    if (APPLE)
        set_target_properties(${target_name} PROPERTIES
            MACOSX_RPATH ON
        )
    else ()
        set_target_properties(${target_name} PROPERTIES
            BUILD_WITH_INSTALL_RPATH 1
            SKIP_BUILD_RPATH FALSE
        )
    endif ()

    set_target_properties(${target_name} PROPERTIES
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )
endmacro()

# Common macro kernels (xcpp)
macro(xeus_cpp_set_kernel_options target_name)
    if (XEUS_CPP_USE_SHARED_XEUS_CPP)
        target_link_libraries(${target_name} PRIVATE xeus-cpp)
        if(CMAKE_DL_LIBS)
            target_link_libraries(${target_name} PRIVATE ${CMAKE_DL_LIBS} util)
        endif()
    else ()
        target_link_libraries(${target_name} PRIVATE xeus-cpp-static)
    endif()

    if(NOT EMSCRIPTEN)
        find_package(Threads)
        target_link_libraries(${target_name} PRIVATE ${CMAKE_THREAD_LIBS_INIT})
    endif()
endmacro()

# Common macro for shared and static library
macro(xeus_cpp_create_target target_name linkage output_name)
    string(TOUPPER "${linkage}" linkage_upper)

    if (NOT ${linkage_upper} MATCHES "^(SHARED|STATIC)$")
        message(FATAL_ERROR "Invalid library linkage: ${linkage}")
    endif ()

    add_library(${target_name} ${linkage_upper} ${XEUS_CPP_SRC} ${XEUS_CPP_HEADERS})
    xeus_cpp_set_common_options(${target_name})

    set_target_properties(${target_name} PROPERTIES
                          PUBLIC_HEADER "${XEUS_CPP_HEADERS}"
                          PREFIX ""
                          VERSION ${${PROJECT_NAME}_VERSION}
                          SOVERSION ${XEUS_CPP_VERSION_MAJOR}
                          OUTPUT_NAME "lib${output_name}")

    target_compile_definitions(${target_name} PUBLIC "XEUS_CPP_EXPORTS")

    target_compile_features(${target_name} PRIVATE cxx_std_17)

    target_include_directories(${target_name}
                               PUBLIC
                               $<BUILD_INTERFACE:${XEUS_CPP_INCLUDE_DIR}>
                               $<INSTALL_INTERFACE:include>)

    if (XEUS_CPP_USE_SHARED_XEUS)
        set(XEUS_CPP_XEUS_TARGET xeus)
    else ()
        set(XEUS_CPP_XEUS_TARGET xeus-static)
    endif ()
    
    #This is a workaround for the issue with the libcurl target on Windows specifically for xassist
    if (WIN32)
        # Set the MSVC runtime library
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

        # Find libcurl
        find_package(CURL REQUIRED)

        # Add CURL_STATICLIB definition if linking statically
        if (CURL_STATICLIB)
            target_compile_definitions(${target_name} PUBLIC CURL_STATICLIB)
        endif()

        # Link against the correct libcurl target
        if (CURL_FOUND)
            target_include_directories(${target_name} PRIVATE ${CURL_INCLUDE_DIRS})
            target_link_libraries(${target_name} PRIVATE ${CURL_LIBRARIES})
        endif()

        # Existing target_link_libraries call, adjusted for clarity
        target_link_libraries(${target_name} PUBLIC ${XEUS_CPP_XEUS_TARGET} clangCppInterOp pugixml argparse::argparse)

        # Ensure all linked libraries use the same runtime library
        if (MSVC)
            target_compile_options(${target_name} PRIVATE "/MD$<$<CONFIG:Debug>:d>")
        endif()
    elseif (NOT EMSCRIPTEN)
        # Curl initialised specifically for xassist
        target_link_libraries(${target_name} PUBLIC ${XEUS_CPP_XEUS_TARGET} clangCppInterOp pugixml argparse::argparse curl)
    else ()
        target_link_libraries(${target_name} PUBLIC ${XEUS_CPP_XEUS_TARGET} clangCppInterOp pugixml argparse::argparse)
    endif()
    
    if (WIN32 OR CYGWIN)
        #
    elseif (APPLE)
        target_link_libraries(${target_name} PRIVATE "-undefined dynamic_lookup")
    endif ()
    if(NOT EMSCRIPTEN)
        find_package(Threads) # TODO: add Threads as a dependence of xeus-static?
        target_link_libraries(${target_name} PRIVATE ${CMAKE_THREAD_LIBS_INIT})
    endif()

endmacro()

# xeus-cpp-headers
# ================

set(XCPP_HEADERS
    include/xcpp/xmime.hpp
    include/xcpp/xdisplay.hpp
)
add_library(xeus-cpp-headers INTERFACE)
set_target_properties(xeus-cpp-headers PROPERTIES PUBLIC_HEADER "${XCPP_HEADERS}")

if (EMSCRIPTEN)
    execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${SYSROOT_PATH}/include/xcpp)

    message(STATUS "Moving headers to ${SYSROOT_PATH}/include/xcpp")
    foreach(header ${XCPP_HEADERS})
        file(COPY ${header} DESTINATION ${SYSROOT_PATH}/include/xcpp)
    endforeach()
else ()
    install(TARGETS xeus-cpp-headers PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xcpp)
endif()

# xeus-cpp
# ========

set(XEUS_CPP_TARGETS "")

if (XEUS_CPP_BUILD_SHARED)
    # Build libraries
    xeus_cpp_create_target(xeus-cpp SHARED xeus-cpp)
    list(APPEND XEUS_CPP_TARGETS xeus-cpp)
endif ()

if (XEUS_CPP_BUILD_STATIC)
    # On Windows, a static library should use a different output name
    # to avoid the conflict with the import library of a shared one.
    if (CMAKE_HOST_WIN32)
        xeus_cpp_create_target(xeus-cpp-static STATIC xeus-cpp-static)
    else ()
        xeus_cpp_create_target(xeus-cpp-static STATIC xeus-cpp)
    endif ()
    list(APPEND XEUS_CPP_TARGETS xeus-cpp-static)
endif ()

# xcpp
# ====

if (XEUS_CPP_BUILD_EXECUTABLE)
    find_package(xeus-zmq ${xeus_zmq_REQUIRED_VERSION} REQUIRED)
    add_executable(xcpp ${XEUS_CPP_MAIN_SRC})
    target_compile_features(xcpp PRIVATE cxx_std_17)
    xeus_cpp_set_common_options(xcpp)
    xeus_cpp_set_kernel_options(xcpp)
    target_link_libraries(xcpp PRIVATE xeus-zmq)
endif()

if(EMSCRIPTEN)
    # Ensure required SDL2 ports (e.g. SDL2) are built before header preloading
    execute_process(
        COMMAND embuilder${EMCC_SUFFIX} build sdl2
        RESULT_VARIABLE SDL2_RESULT
    )
    if(NOT SDL2_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to run 'embuilder build sdl2'")
    endif()
    include(WasmBuildOptions)
    find_package(xeus-lite ${xeus_lite_REQUIRED_VERSION} REQUIRED)
    add_executable(xcpp src/main_emscripten_kernel.cpp )
    target_link_libraries(xcpp PRIVATE xeus-lite)
    xeus_cpp_set_kernel_options(xcpp)
    xeus_wasm_compile_options(xcpp)
    xeus_wasm_link_options(xcpp "web,worker")
    string(REPLACE "@" "@@" ESCAPED_SYSROOT_PATH "${SYSROOT_PATH}")
    string(REPLACE "@" "@@" ESCAPED_XEUS_CPP_RESOURCE_DIR "${XEUS_CPP_RESOURCE_DIR}")
    string(REPLACE "\\" "/" ESCAPED_XEUS_CPP_RESOURCE_DIR "${ESCAPED_XEUS_CPP_RESOURCE_DIR}")
    string(REPLACE "\\" "/" ESCAPED_SYSROOT_PATH "${ESCAPED_SYSROOT_PATH}")
    target_link_options(xcpp
        PUBLIC "SHELL: -s USE_SDL=2"
        PUBLIC "SHELL: --preload-file ${ESCAPED_SYSROOT_PATH}/include@/include"
        PUBLIC "SHELL: --preload-file ${ESCAPED_XEUS_CPP_RESOURCE_DIR}@/${CMAKE_INSTALL_LIBDIR}/clang/${CPPINTEROP_LLVM_VERSION_MAJOR}"
        PUBLIC "SHELL: --post-js ${CMAKE_CURRENT_SOURCE_DIR}/wasm_patches/post.js"
    )
endif()
# Tests
# =====

if(XEUS_CPP_BUILD_TESTS)
    add_subdirectory(test)
endif()

# Installation
# ============
include(CMakePackageConfigHelpers)

set(XEUS_CPP_CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE STRING "install path for xeus-cppConfig.cmake")

install(DIRECTORY ${XCPP_TAGFILES_DIR}
        DESTINATION ${XEUS_CPP_DATA_DIR})

install(DIRECTORY ${XCPP_TAGCONFS_DIR}
        DESTINATION ${XEUS_CPP_CONF_DIR})

# Install xeus-cpp and xeus-cpp-static
if (XEUS_CPP_BUILD_SHARED)
    install(TARGETS ${XEUS_CPP_TARGETS}
            EXPORT ${PROJECT_NAME}-targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xeus-cpp)

    # Makes the project importable from the build directory
    export(EXPORT ${PROJECT_NAME}-targets
           FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake")
endif ()

# Install xcpp
if (XEUS_CPP_BUILD_EXECUTABLE)
    install(TARGETS xcpp
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(XEUS_CPP_BUILD_EXECUTABLE OR EMSCRIPTEN)
    # Configuration and data directories for jupyter and xeus-cpp
    set(XJUPYTER_DATA_DIR "share/jupyter"    CACHE STRING "Jupyter data directory")

    # Install xcpp Jupyter kernelspec
    set(KERNELSPEC_DIR ${CMAKE_CURRENT_BINARY_DIR}/share/jupyter/kernels)
    install(DIRECTORY ${KERNELSPEC_DIR}
            DESTINATION ${XJUPYTER_DATA_DIR}
            PATTERN "*.in" EXCLUDE)


    # Extra path for installing Jupyter kernelspec
    if (XEXTRA_JUPYTER_DATA_DIR)
        install(DIRECTORY ${KERNELSPEC_DIR}
                DESTINATION ${XEXTRA_JUPYTER_DATA_DIR}
                PATTERN "*.in" EXCLUDE)
    endif ()
endif ()

# Configure 'xeus-cppConfig.cmake' for a build tree
set(XEUS_CPP_CONFIG_CODE "####### Expanded from \@XEUS_CPP_CONFIG_CODE\@ #######\n")
set(XEUS_CPP_CONFIG_CODE "${XEUS_CPP_CONFIG_CODE}set(CMAKE_MODULE_PATH \"${CMAKE_CURRENT_SOURCE_DIR}/cmake;\${CMAKE_MODULE_PATH}\")\n")
set(XEUS_CPP_CONFIG_CODE "${XEUS_CPP_CONFIG_CODE}##################################################")
configure_package_config_file(${PROJECT_NAME}Config.cmake.in
                              "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                              INSTALL_DESTINATION ${PROJECT_BINARY_DIR})

# Configure 'xeus-cppConfig.cmake.in for an install tree
set(XEUS_CPP_CONFIG_CODE "")
configure_package_config_file(${PROJECT_NAME}Config.cmake.in
                              "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake"
                              INSTALL_DESTINATION ${XEUS_CPP_CMAKECONFIG_INSTALL_DIR})

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
                                 VERSION ${${PROJECT_NAME}_VERSION}
                                 COMPATIBILITY AnyNewerVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}Config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
              DESTINATION ${XEUS_CPP_CMAKECONFIG_INSTALL_DIR})

if (XEUS_CPP_BUILD_SHARED)
    install(EXPORT ${PROJECT_NAME}-targets
            FILE ${PROJECT_NAME}Targets.cmake
            DESTINATION ${XEUS_CPP_CMAKECONFIG_INSTALL_DIR})
endif ()

if(EMSCRIPTEN)
    install(FILES
            "$<TARGET_FILE_DIR:xcpp>/xcpp.js"
            "$<TARGET_FILE_DIR:xcpp>/xcpp.wasm"
            "$<TARGET_FILE_DIR:xcpp>/xcpp.data"
            DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()
