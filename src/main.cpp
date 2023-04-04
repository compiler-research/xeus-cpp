/***************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors
 *
 * Distributed under the terms of the BSD 3-Clause License.
 *
 * The full license is in the file LICENSE, distributed with this software.
 ****************************************************************************/

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#ifdef __GNUC__
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#include "xeus-zmq/xserver_zmq.hpp"
#include "xeus/xkernel.hpp"
#include "xeus/xkernel_configuration.hpp"


#include "xeus-cpp/xeus_cpp_config.hpp"
#include "xeus-cpp/xinterpreter.hpp"


#ifdef __GNUC__
void handler(int sig)
{
    void* array[10];

    // get void*'s for all entries on the stack
    size_t size = backtrace(array, 10);

    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}
#endif

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

std::string extract_filename(int argc, char* argv[])
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

int main(int argc, char* argv[])
{
    if (should_print_version(argc, argv))
    {
        std::clog << "xcpp " << XEUS_CPP_VERSION << std::endl;
        return 0;
    }

    // If we are called from the Jupyter launcher, silence all logging. This
    // is important for a JupyterHub configured with cleanup_servers = False:
    // Upon restart, spawned single-user servers keep running but without the
    // std* streams. When a user then tries to start a new kernel, xcpp
    // will get a SIGPIPE and exit.
    if (std::getenv("JPY_PARENT_PID") != NULL)
    {
        std::clog.setstate(std::ios_base::failbit);
    }

    // Registering SIGSEGV handler
#ifdef __GNUC__
    std::clog << "registering handler for SIGSEGV" << std::endl;
    signal(SIGSEGV, handler);
#endif

    auto context = xeus::make_context<zmq::context_t>();

    // Instantiating the xeus xinterpreter
    using interpreter_ptr = std::unique_ptr<xcpp::interpreter>;
    interpreter_ptr interpreter = interpreter_ptr(new xcpp::interpreter());


    std::string connection_filename = extract_filename(argc, argv);

    if (!connection_filename.empty())
    {
        xeus::xconfiguration config = xeus::load_configuration(connection_filename);
        xeus::xkernel kernel(
            config,
            xeus::get_user_name(),
            std::move(context),
            std::move(interpreter),
            xeus::make_xserver_zmq
        );

        std::cout << "Starting xcpp kernel...\n\n"
                     "If you want to connect to this kernel from an other client, you can use"
                     " the "
                         + connection_filename + " file."
                  << std::endl;

        kernel.start();
    }
    else
    {
        xeus::xkernel
            kernel(xeus::get_user_name(), std::move(context), std::move(interpreter), xeus::make_xserver_zmq);

        const auto& config = kernel.get_config();
        std::cout << "Starting xcpp kernel...\n\n"
                     "If you want to connect to this kernel from an other client, just copy"
                     " and paste the following content inside of a `kernel.json` file. And then run for example:\n\n"
                     "# jupyter console --existing kernel.json\n\n"
                     "kernel.json\n```\n{\n"
                     "    \"transport\": \""
                         + config.m_transport
                         + "\",\n"
                           "    \"ip\": \""
                         + config.m_ip
                         + "\",\n"
                           "    \"control_port\": "
                         + config.m_control_port
                         + ",\n"
                           "    \"shell_port\": "
                         + config.m_shell_port
                         + ",\n"
                           "    \"stdin_port\": "
                         + config.m_stdin_port
                         + ",\n"
                           "    \"iopub_port\": "
                         + config.m_iopub_port
                         + ",\n"
                           "    \"hb_port\": "
                         + config.m_hb_port
                         + ",\n"
                           "    \"signature_scheme\": \""
                         + config.m_signature_scheme
                         + "\",\n"
                           "    \"key\": \""
                         + config.m_key
                         + "\"\n"
                           "}\n```"
                  << std::endl;

        kernel.start();
    }

    return 0;
}
