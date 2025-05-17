#include "xeus-cpp/xdebugger.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "nlohmann/json.hpp"
#include "xdebuglldb_client.hpp"
#include "xeus-zmq/xmiddleware.hpp"
#include "xeus/xsystem.hpp"
#include "xinternal_utils.hpp"

using namespace std::placeholders;

namespace xcpp
{
    debugger::debugger(
        xeus::xcontext& context,
        const xeus::xconfiguration& config,
        const std::string& user_name,
        const std::string& session_id,
        const nl::json& debugger_config
    )
        : xdebugger_base(context)
        , p_debuglldb_client(new xdebuglldb_client(
              context,
              config,
              xeus::get_socket_linger(),
              xdap_tcp_configuration(xeus::dap_tcp_type::client, xeus::dap_init_type::parallel, user_name, session_id),
              get_event_callback()
          ))
        , m_lldb_host("127.0.0.1")
        , m_lldb_port("")
        , m_debugger_config(debugger_config)
        , m_is_running(false)
    {
        // Register request handlers
        register_request_handler(
            "inspectVariables",
            std::bind(&debugger::inspect_variables_request, this, _1),
            false
        );
        register_request_handler("stackTrace", std::bind(&debugger::stack_trace_request, this, _1), false);
        register_request_handler("attach", std::bind(&debugger::attach_request, this, _1), true);
        register_request_handler(
            "configurationDone",
            std::bind(&debugger::configuration_done_request, this, _1),
            true
        );

        std::cout << "Debugger initialized with config: " << m_debugger_config.dump() << std::endl;
    }

    debugger::~debugger()
    {
        delete p_debuglldb_client;
        p_debuglldb_client = nullptr;
    }

    bool debugger::start_lldb()
    {
        // Find a free port for LLDB-DAP
        m_lldb_port = xeus::find_free_port(100, 9999, 10099);
        if (m_lldb_port.empty())
        {
            std::cout << "Failed to find a free port for LLDB-DAP" << std::endl;
            return false;
        }

        // Log debugger configuration if XEUS_LOG is set
        if (std::getenv("XEUS_LOG") != nullptr)
        {
            std::ofstream out("xeus.log", std::ios_base::app);
            out << "===== DEBUGGER CONFIG =====" << std::endl;
            out << m_debugger_config.dump() << std::endl;
        }

        // Construct LLDB-DAP command
        std::string lldb_command = "lldb-dap --port " + m_lldb_port;

        std::cout << "lldb-dap command: " << lldb_command << std::endl;

        // Optionally, append additional configuration from m_debugger_config
        auto it = m_debugger_config.find("lldb");
        if (it != m_debugger_config.end() && it->is_object())
        {
            if (it->contains("initCommands"))
            {
                std::cout << "Adding init commands to lldb-dap command" << std::endl;
                for (const auto& cmd : it->at("initCommands").get<std::vector<std::string>>())
                {
                    std::cout << "Adding command: " << cmd << std::endl;
                    lldb_command += " --init-command \"" + cmd + "\"";
                }
            }
        }

        // Start LLDB-DAP process
        // Note: Using std::system for simplicity; replace with a process library in production
        std::string log_dir = xeus::get_temp_directory_path() + "/xcpp_debug_logs_"
                              + std::to_string(xeus::get_current_pid());
        xeus::create_directory(log_dir);
        std::string log_file = log_dir + "/lldb-dap.log";

        // Fork to start lldb-dap
        pid_t pid = fork();
        if (pid == 0)
        {  // Child process
            // Redirect stdin, stdout, stderr to /dev/null and log file
            int fd = open("/dev/null", O_RDONLY);
            dup2(fd, STDIN_FILENO);
            close(fd);

            fd = open(log_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);

            // Execute lldb-dap
            execlp(
                "lldb-dap",
                "lldb-dap",
                "--port",
                m_lldb_port.c_str(),
                "--init-command",
                "settings set plugin.jit-loader.gdb.enable on",
                NULL
            );

            // If execlp fails
            std::cout << xcpp::red_text("execlp failed for lldb-dap") << std::endl;
            exit(1);
        }
        else if (pid > 0)
        {  // Parent process
            std::cout << xcpp::green_text("LLDB-DAP process started, PID: ") << pid << std::endl;

            // Check if process is running
            int status;
            if (waitpid(pid, &status, WNOHANG) != 0)
            {
                std::cout << xcpp::red_text("LLDB-DAP process exited early") << std::endl;
                return false;
            }
        }
        else
        {
            std::cout << xcpp::red_text("fork failed") << std::endl;
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

        try
        {
            std::string endpoint = "tcp://" + m_lldb_host + ":" + m_lldb_port;
            std::cout << xcpp::blue_text("Testing connection to LLDB-DAP at ") << endpoint << std::endl;
            if (!p_debuglldb_client->test_connection(endpoint))
            {
                std::cout << xcpp::red_text("LLDB-DAP server not responding at ") << endpoint << std::endl;
                return false;
            }
            std::cout << xcpp::green_text("Connected to LLDB-DAP at ") << endpoint << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cout << xcpp::red_text("Exception while connecting to LLDB-DAP: ") << e.what() << std::endl;
            return false;
        }

        m_is_running = true;
        return true;
    }

    bool debugger::start()
    {
        // Create temporary directory for logs
        std::cout << "Starting debugger..." << std::endl;
        std::string temp_dir = xeus::get_temp_directory_path();
        std::string log_dir = temp_dir + "/xcpp_debug_logs_" + std::to_string(xeus::get_current_pid());
        xeus::create_directory(log_dir);

        // Start LLDB-DAP
        static bool lldb_started = start_lldb();
        if (!lldb_started)
        {
            std::cout << "Failed to start LLDB-DAP" << std::endl;
            return false;
        }

        // Bind xeus debugger sockets
        std::string controller_end_point = xeus::get_controller_end_point("debugger");
        std::string controller_header_end_point = xeus::get_controller_end_point("debugger_header");
        std::string publisher_end_point = xeus::get_publisher_end_point();

        bind_sockets(controller_header_end_point, controller_end_point);

        // Start LLDB-DAP client in a separate thread
        std::string lldb_endpoint = "tcp://" + m_lldb_host + ":" + m_lldb_port;
        std::thread client(
            &xdebuglldb_client::start_debugger,
            p_debuglldb_client,
            lldb_endpoint,
            publisher_end_point,
            controller_end_point,
            controller_header_end_point
        );
        client.detach();

        // Send initial DAP request to verify connection
        send_recv_request("REQ");

        // Create temporary folder for cell code
        std::string tmp_folder = get_tmp_prefix();
        xeus::create_directory(tmp_folder);

        return true;
    }

    // Dummy implementations for other methods
    nl::json debugger::inspect_variables_request(const nl::json& message)
    {
        // Placeholder DAP response
        nl::json reply = {
            {"type", "response"},
            {"request_seq", message["seq"]},
            {"success", false},
            {"command", message["command"]},
            {"message", "inspectVariables not implemented"}
        };
        return reply;
    }

    nl::json debugger::stack_trace_request(const nl::json& message)
    {
        // Placeholder DAP response
        nl::json reply = {
            {"type", "response"},
            {"request_seq", message["seq"]},
            {"success", false},
            {"command", message["command"]},
            {"message", "stackTrace not implemented"},
            {"body", {{"stackFrames", {}}}}
        };
        return reply;
    }

    nl::json debugger::attach_request(const nl::json& message)
    {
        // Placeholder DAP response
        nl::json reply = {
            {"type", "response"},
            {"request_seq", message["seq"]},
            {"success", false},
            {"command", message["command"]},
            {"message", "attach not implemented"}
        };
        return reply;
    }

    nl::json debugger::configuration_done_request(const nl::json& message)
    {
        // Minimal DAP response to allow DAP workflow to proceed
        nl::json reply = {
            {"type", "response"},
            {"request_seq", message["seq"]},
            {"success", true},
            {"command", message["command"]}
        };
        return reply;
    }

    nl::json debugger::variables_request_impl(const nl::json& message)
    {
        // Placeholder DAP response
        nl::json reply = {
            {"type", "response"},
            {"request_seq", message["seq"]},
            {"success", false},
            {"command", message["command"]},
            {"message", "variables not implemented"},
            {"body", {{"variables", {}}}}
        };
        return reply;
    }

    void debugger::stop()
    {
        // Placeholder: Log stop attempt
        std::cout << "Debugger stop called" << std::endl;
        std::string controller_end_point = xeus::get_controller_end_point("debugger");
        std::string controller_header_end_point = xeus::get_controller_end_point("debugger_header");
        unbind_sockets(controller_header_end_point, controller_end_point);
    }

    xeus::xdebugger_info debugger::get_debugger_info() const
    {
        // Placeholder debugger info
        return xeus::xdebugger_info(
            xeus::get_tmp_hash_seed(),
            get_tmp_prefix(),
            get_tmp_suffix(),
            true,  // Supports exceptions
            {"C++ Exceptions"},
            true  // Supports breakpoints
        );
    }

    std::string debugger::get_cell_temporary_file(const std::string& code) const
    {
        // Placeholder: Return a dummy temporary file path
        std::string tmp_file = get_tmp_prefix() + "/cell_tmp.cpp";
        std::ofstream out(tmp_file);
        out << code;
        out.close();
        return tmp_file;
    }

    std::unique_ptr<xeus::xdebugger> make_cpp_debugger(
        xeus::xcontext& context,
        const xeus::xconfiguration& config,
        const std::string& user_name,
        const std::string& session_id,
        const nl::json& debugger_config
    )
    {
        std::cout << "Creating C++ debugger" << std::endl;
        return std::unique_ptr<xeus::xdebugger>(
            new debugger(context, config, user_name, session_id, debugger_config)
        );
    }
}