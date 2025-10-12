/****************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include <signal.h>

#ifdef __GNUC__
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#include "nlohmann/json.hpp"
#include <xeus/xhelper.hpp>
#include <xeus/xkernel.hpp>
#include <xeus/xkernel_configuration.hpp>
#include "xeus-zmq/xzmq_context.hpp"
#include <xeus-zmq/xserver_zmq.hpp>
#include "xeus-zmq/xserver_zmq_split.hpp"
#include "xeus-cpp/xeus_cpp_config.hpp"
#include "xeus-cpp/xinterpreter.hpp"
#include "xeus-cpp/xutils.hpp"
#include "xeus-cpp/xdebugger.hpp"

namespace nl = nlohmann;

int main(int argc, char* argv[])
{
    if (xeus::should_print_version(argc, argv))
    {
        std::clog << "xcpp " << XEUS_CPP_VERSION << std::endl;
        return 0;
    }

    // If we are called from the Jupyter launcher, silence all logging. This
    // is important for a JupyterHub configured with cleanup_servers = False:
    // Upon restart, spawned single-user servers keep running but without the
    // std* streams. When a user then tries to start a new kernel, xeus-cpp
    // will get a SIGPIPE when writing to any of these and exit.
    if (std::getenv("JPY_PARENT_PID") != NULL)
    {
        std::clog.setstate(std::ios_base::failbit);
    }

    // Registering SIGSEGV handler
#ifdef __GNUC__
    std::clog << "registering handler for SIGSEGV" << std::endl;
    signal(SIGSEGV, xcpp::handler);

    // Registering SIGINT and SIGKILL handlers
    signal(SIGKILL, xcpp::stop_handler);
#endif
    signal(SIGINT, xcpp::stop_handler);

    std::string file_name = xeus::extract_filename(argc, argv);
    auto interpreter = std::make_unique<xcpp::interpreter>(argc, argv);
    std::unique_ptr<xeus::xcontext> context = xeus::make_zmq_context();

    nl::json debugger_config;
    debugger_config["lldb"]["initCommands"] = {
        "settings set plugin.jit-loader.gdb.enable on"
    };
    debugger_config["interpreter_ptr"] = reinterpret_cast<std::uintptr_t>(interpreter.get());

    if (!file_name.empty())
    {
        xeus::xconfiguration config = xeus::load_configuration(file_name);

        std::clog << "Instantiating kernel" << std::endl;
        xeus::xkernel kernel(
            config,
            xeus::get_user_name(),
            std::move(context),
            std::move(interpreter),
            xeus::make_xserver_shell_main,
            xeus::make_in_memory_history_manager(),
            xeus::make_console_logger(
                xeus::xlogger::msg_type,
                xeus::make_file_logger(xeus::xlogger::content, "xeus.log")
            ),
            xcpp::make_cpp_debugger,
            debugger_config
        );

        std::clog << "Starting xcpp kernel...\n\n"
                     "If you want to connect to this kernel from an other client, you can use"
                     " the " + file_name + " file."
                  << std::endl;

        kernel.start();
    }
    else
    {
        xeus::xkernel kernel(
            xeus::get_user_name(),
            std::move(context),
            std::move(interpreter),
            xeus::make_xserver_shell_main,
            xeus::make_in_memory_history_manager(),
            xeus::make_console_logger(
                xeus::xlogger::msg_type,
                xeus::make_file_logger(xeus::xlogger::content, "xeus.log")
            ),
            xcpp::make_cpp_debugger,
            debugger_config
        );

        std::cout << "Getting config" << std::endl;
        const auto& config = kernel.get_config();
        std::clog << "Starting xcpp kernel...\n\n"
                     "If you want to connect to this kernel from an other client, just copy"
                     " and paste the following content inside of a `kernel.json` file. And then run for example:\n\n"
                     "# jupyter console --existing kernel.json\n\n"
                     "kernel.json\n```\n{\n"
                     "    \"transport\": \"" + config.m_transport + "\",\n"
                     "    \"ip\": \"" + config.m_ip + "\",\n"
                     "    \"control_port\": " + config.m_control_port + ",\n"
                     "    \"shell_port\": " + config.m_shell_port + ",\n"
                     "    \"stdin_port\": " + config.m_stdin_port + ",\n"
                     "    \"iopub_port\": " + config.m_iopub_port + ",\n"
                     "    \"hb_port\": " + config.m_hb_port + ",\n"
                     "    \"signature_scheme\": \"" + config.m_signature_scheme + "\",\n"
                     "    \"key\": \"" + config.m_key + "\"\n"
                     "}\n```"
                  << std::endl;

        kernel.start();
    }

    return 0;
}