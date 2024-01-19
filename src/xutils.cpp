/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include <signal.h>

#ifdef __GNUC__
#include <stdio.h>
#ifndef XEUS_CPP_EMSCRIPTEN_WASM_BUILD
#include <execinfo.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#endif


#include "xeus-cpp/xutils.hpp"
#include "xeus-cpp/xinterpreter.hpp"

namespace xcpp
{

#if defined(__GNUC__) && !defined(XEUS_CPP_EMSCRIPTEN_WASM_BUILD)
    void handler(int sig)
    {
        void* array[10];

        // get void*'s for all entries on the stack
        std::size_t size = backtrace(array, 10);

        // print out all the frames to stderr
        fprintf(stderr, "Error: signal %d:\n", sig);
        backtrace_symbols_fd(array, size, STDERR_FILENO);
        exit(1);
    }
#endif

    void stop_handler(int /*sig*/)
    {
        exit(0);
    }

    bool should_print_version(int argc, char* argv[])
    {
        for (int i = 0; i < argc; ++i)
        {
            if (std::string(argv[i]) == "--version")
            {
                return true;
            }
        }
        return false;
    }

    std::string extract_filename(int &argc, char* argv[])
    {
        std::string res = "";
        for (int i = 0; i < argc; ++i)
        {
            if ((std::string(argv[i]) == "-f") && (i + 1 < argc))
            {
                res = argv[i + 1];
                for (int j = i; j < argc - 2; ++j)
                {
                    argv[j] = argv[j + 2];
                }
                argc -= 2;
                break;
            }
        }
        return res;
    }

    interpreter_ptr build_interpreter(int argc, char** argv)
    {
        std::vector<const char*> interpreter_argv(argc);
        interpreter_argv[0] = "xeus-cpp";

        // Copy all arguments in the new vector excepting the process name.
        for (int i = 1; i < argc; i++)
        {
            interpreter_argv[i] = argv[i];
        }

        interpreter_ptr interp_ptr = std::make_unique<interpreter>(argc, interpreter_argv.data());
        return interp_ptr;
    }

}