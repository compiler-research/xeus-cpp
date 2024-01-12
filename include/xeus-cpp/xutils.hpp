/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#ifndef XEUS_CPP_UTILS_HPP
#define XEUS_CPP_UTILS_HPP

#include "xinterpreter.hpp"

using interpreter_ptr = std::unique_ptr<xcpp::interpreter>;

namespace xcpp
{

#ifdef __GNUC__
    XEUS_CPP_API
    void handler(int sig);
#endif

    XEUS_CPP_API
    void stop_handler(int sig);

    XEUS_CPP_API
    bool should_print_version(int argc, char* argv[]);

    XEUS_CPP_API
    std::string extract_filename(int &argc, char* argv[]);

    XEUS_CPP_API
    interpreter_ptr build_interpreter(int argc, char** argv);
}

#endif
