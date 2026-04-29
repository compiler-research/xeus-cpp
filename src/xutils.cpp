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
#ifndef __EMSCRIPTEN__
#include <execinfo.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#endif

#include "xeus/xsystem.hpp"

#include "xeus-cpp/xutils.hpp"
#include "xeus-cpp/xinterpreter.hpp"

namespace xcpp
{

#if defined(__GNUC__) && !defined(__EMSCRIPTEN__)
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

    std::string retrieve_tagconf_dir()
    {
        const char* tagconf_dir_env = std::getenv("XCPP_TAGCONFS_DIR");
        if (tagconf_dir_env != nullptr)
        {
            return tagconf_dir_env;
        }

        std::string prefix = xeus::prefix_path();

#if defined(_WIN32)
        const char separator = '\\';
#else
        const char separator = '/';
#endif

        return prefix + "etc" + separator + "xeus-cpp" + separator + "tags.d";
    }

    std::string retrieve_tagfile_dir()
    {
        const char* tagfile_dir_env = std::getenv("XCPP_TAGFILES_DIR");
        if (tagfile_dir_env != nullptr)
        {
            return tagfile_dir_env;
        }

        std::string prefix = xeus::prefix_path();

#if defined(_WIN32)
        const char separator = '\\';
#else
        const char separator = '/';
#endif

        return prefix + "share" + separator + "xeus-cpp" + separator + "tagfiles";
    }
}
