#############################################################################
# Copyright (c) 2023, xeus-cpp contributors                                 #
#                                                                           #
# Distributed under the terms of the BSD 3-Clause License.                  #
#                                                                           #
# The full license is in the file LICENSE, distributed with this software.  #
#############################################################################

cmake_minimum_required(VERSION 3.4.3)
project(xeus-cpp)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

set(XEUS_CPP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Versionning
# ===========

file(STRINGS "${XEUS_CPP_INCLUDE_DIR}/xeus-cpp/xeus_cpp_config.hpp" version_defines
     REGEX "#define XEUS_CPP_VERSION_(MAJOR|MINOR|PATCH)")
foreach (ver ${version_defines})
    if (ver MATCHES "#define XEUS_CPP_VERSION_(MAJOR|MINOR|PATCH) +([^ ]+)$")
        set(XEUS_CPP_VERSION_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE INTERNAL "")
    endif ()
endforeach ()
set(${PROJECT_NAME}_VERSION
    ${XEUS_CPP_VERSION_MAJOR}.${XEUS_CPP_VERSION_MINOR}.${XEUS_CPP_VERSION_PATCH})
message(STATUS "Building xeus-cpp v${${PROJECT_NAME}_VERSION}")

# Configuration
# =============

include(GNUInstallDirs)

if (NOT DEFINED XEUS_CPP_KERNELSPEC_PATH)
    set(XEUS_CPP_KERNELSPEC_PATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/")
endif ()

configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels/xcpp/kernel.json.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels/xcpp/kernel.json"
)

# Build options
# =============

option(XEUS_CPP_BUILD_STATIC "Build xeus-cpp static library" ON)
OPTION(XEUS_CPP_BUILD_SHARED "Split xcpp build into executable and library" ON)
OPTION(XEUS_CPP_BUILD_EXECUTABLE "Build the xcpp executable" ON)

OPTION(XEUS_CPP_USE_SHARED_XEUS "Link xcpp  with the xeus shared library (instead of the static library)" ON)
OPTION(XEUS_CPP_USE_SHARED_XEUS_CPP "Link xcpp  with the xeus shared library (instead of the static library)" ON)



if(EMSCRIPTEN)
    add_compile_definitions(XEUS_CPP_EMSCRIPTEN_WASM_BUILD)
    message("Build with emscripten")
    SET(XEUS_CPP_BUILD_STATIC ON)
    SET(XEUS_CPP_BUILD_SHARED OFF)
    SET(XEUS_CPP_BUILD_EXECUTABLE OFF)
    SET(XEUS_CPP_USE_SHARED_XEUS OFF)
    SET(XEUS_CPP_USE_SHARED_XEUS_CPP OFF)
    SET(XEUS_CPP_BUILD_TESTS OFF)
endif()


# Dependencies
# ============

set(xtl_REQUIRED_VERSION 0.7.0)
set(xeus_REQUIRED_VERSION 3.0.5)

if (NOT TARGET xtl)
    find_package(xtl ${xtl_REQUIRED_VERSION} REQUIRED)
endif ()
if (NOT TARGET xeus AND NOT TARGET xeus-static)
    find_package(xeus ${xeus_REQUIRED_VERSION} REQUIRED)
endif ()

# Flags
# =====
include(CheckCXXCompilerFlag)

if (MSVC)
  add_compile_options(/wd4251 /wd4141)
  add_compile_options(/wd4018 /wd4267 /wd4715 /wd4146 /wd4129)
  add_compile_options(/EHsc)
else()
  add_compile_options(-fexceptions)
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Intel")
    if(NOT EMSCRIPTEN)
        add_compile_options(-Wunused-parameter -Wextra -Wreorder)
    endif()
    

    CHECK_CXX_COMPILER_FLAG("-std=c++17" HAS_CPP_17_FLAG)
    if (HAS_CPP_17_FLAG)
        add_compile_options(-std=c++17)
    else ()
        message(FATAL_ERROR "Unsupported compiler -- xeus requires C++17 support!")
    endif ()
endif ()

if(EMSCRIPTEN)
    # ENV (https://github.com/emscripten-core/emscripten/commit/6d9681ad04f60b41ef6345ab06c29bbc9eeb84e0)
    set(EMSCRIPTEN_FEATURES "${EMSCRIPTEN_FEATURES} -s \"EXTRA_EXPORTED_RUNTIME_METHODS=[ENV']\"")
endif()

find_package(Clang REQUIRED)
include(AddLLVM)
include(HandleLLVMOptions)
add_definitions(${LLVM_DEFINITIONS})
find_package(argparse REQUIRED)
find_package(pugixml REQUIRED)

# Source files
# ============

set(XEUS_CPP_HEADERS
    include/xeus-cpp/xeus_cpp_config.hpp
    include/xeus-cpp/xinterpreter.hpp
)

set(XEUS_CPP_SRC
    src/xholder.cpp
    src/xinput.cpp
    src/xinterpreter.cpp
    src/xoptions.cpp
    src/xparser.cpp
)

if(EMSCRIPTEN)
    list(APPEND XEUS_CPP_SRC src/xinterpreter_wasm.cpp)
endif()

set(XEUS_CPP_MAIN_SRC
    src/main.cpp
)

# Targets and link - Macros
# =========================

include(CheckCXXCompilerFlag)

string(TOUPPER "${CMAKE_BUILD_TYPE}" U_CMAKE_BUILD_TYPE)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib; ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

macro(xeus_cpp_set_common_options target_name)
    if (MSVC)
        target_compile_options(${target_name} PUBLIC /wd4251 /wd4141)
        target_compile_options(${target_name} PUBLIC /wd4018 /wd4267 /wd4715 /wd4146 /wd4129)
    endif ()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
        CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR
        CMAKE_CXX_COMPILER_ID MATCHES "Intel")

        target_compile_options(${target_name} PUBLIC -Wunused-parameter -Wextra -Wreorder)

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

    target_link_libraries(${target_name} PUBLIC ${XEUS_CPP_XEUS_TARGET} clangInterpreter pugixml argparse::argparse xtl)
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

install(TARGETS xeus-cpp-headers PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xcpp)

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
    find_package(xeus-zmq 1.0.2 REQUIRED)
    add_executable(xcpp ${XEUS_CPP_MAIN_SRC})
    target_compile_features(xcpp PRIVATE cxx_std_17)
    xeus_cpp_set_common_options(xcpp)
    xeus_cpp_set_kernel_options(xcpp)
    target_link_libraries(xcpp PRIVATE xeus-zmq)
endif()

if(EMSCRIPTEN)
    include(WasmBuildOptions)
    find_package(xeus-lite REQUIRED)
    add_executable(xcpp src/main_emscripten_kernel.cpp )
    target_link_libraries(xcpp PRIVATE xeus-lite)
    xeus_cpp_set_kernel_options(xcpp)
    xeus_wasm_compile_options(xcpp)
    xeus_wasm_link_options(xcpp "web,worker")
endif()

# Installation
# ============
include(CMakePackageConfigHelpers)

set(XEUS_CPP_CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE STRING "install path for xeus-cppConfig.cmake")

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
    set(KERNELSPEC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/share/jupyter/kernels)
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

# Install xeus-cpp tag files
set(XEUS_CPP_DATA_DIR "share/xeus-cpp" CACHE STRING "xeus-cpp data directory")
set(XCPP_TAGFILES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/share/xeus-cpp/tagfiles)
install(DIRECTORY ${XCPP_TAGFILES_DIR}
        DESTINATION ${XEUS_CPP_DATA_DIR})

set(XEUS_CPP_CONF_DIR "etc/xeus-cpp"   CACHE STRING "xeus-cpp configuration directory")
set(XCPP_TAGCONFS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/etc/xeus-cpp/tags.d)
install(DIRECTORY ${XCPP_TAGCONFS_DIR}
        DESTINATION ${XEUS_CPP_CONF_DIR})

# Add definitions for the kernel to find tagfiles.
add_definitions(-DXCPP_TAGFILES_DIR="${CMAKE_INSTALL_PREFIX}/${XEUS_CPP_DATA_DIR}/tagfiles")
if(CMAKE_INSTALL_PREFIX STREQUAL "/usr")
    # install into /etc instead of /usr/etc
    add_definitions(-DXCPP_TAGCONFS_DIR="/${XEUS_CPP_CONF_DIR}/tags.d")
else()
    add_definitions(-DXCPP_TAGCONFS_DIR="${CMAKE_INSTALL_PREFIX}/${XEUS_CPP_CONF_DIR}/tags.d")
endif()

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
    install(TARGETS xcpp
            ARCHIVE DESTINATION ${XEUS_CPP_CMAKECONFIG_INSTALL_DIR})

    install(FILES
            "$<TARGET_FILE_DIR:xcpp>/xcpp.js"
            "$<TARGET_FILE_DIR:xcpp>/xcpp.wasm"
            DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()

if(XEUS_CPP_INCLUDE_DOCS)
    add_subdirectory(docs)
endif()