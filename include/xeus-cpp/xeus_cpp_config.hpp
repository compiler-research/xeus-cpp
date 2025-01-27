/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#ifndef XEUS_CPP_CONFIG_HPP
#define XEUS_CPP_CONFIG_HPP

// Project version
#define XEUS_CPP_VERSION_MAJOR 0
#define XEUS_CPP_VERSION_MINOR 7
#define XEUS_CPP_VERSION_PATCH 0
#define XEUS_CPP_VERSION_LABEL dev

// Composing the version string from major, minor and patch
#define XEUS_CPP_CONCATENATE(A, B) XEUS_CPP_CONCATENATE_IMPL(A, B)
#define XEUS_CPP_CONCATENATE_IMPL(A, B) A##B
#define XEUS_CPP_STRINGIFY(a) XEUS_CPP_STRINGIFY_IMPL(a)
#define XEUS_CPP_STRINGIFY_IMPL(a) #a

#define XEUS_CPP_VERSION                                                                                      \
    XEUS_CPP_STRINGIFY(XEUS_CPP_CONCATENATE(                                                                  \
        XEUS_CPP_VERSION_MAJOR,                                                                               \
        XEUS_CPP_CONCATENATE(                                                                                 \
                .,                                                                                            \
                XEUS_CPP_CONCATENATE(XEUS_CPP_VERSION_MINOR, XEUS_CPP_CONCATENATE(., XEUS_CPP_VERSION_PATCH)) \
        )                                                                                                     \
    ))

#ifdef _WIN32
#ifdef XEUS_CPP_EXPORTS
#define XEUS_CPP_API __declspec(dllexport)
#else
#define XEUS_CPP_API __declspec(dllimport)
#endif
#else
#define XEUS_CPP_API
#endif

#endif
