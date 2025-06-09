#include "xeus-cpp/xdebugger.hpp"

#include <arpa/inet.h>  // For inet_pton(), htons()
#include <chrono>       // For std::chrono (used in sleep_for)
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <netinet/in.h>  // For sockaddr_in, AF_INET
#include <sys/socket.h>  // For socket(), connect(), send(), recv()
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

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
        std::cout << "Stopping debugger..........." << std::endl;
        delete p_debuglldb_client;
        p_debuglldb_client = nullptr;
    }

    bool debugger::start_lldb()
    {
        std::cout << "debugger::start_lldb" << std::endl;

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

        // Build C++ code to start LLDB-DAP process
        std::string code = "#include <iostream>\n";
        code += "#include <string>\n";
        code += "#include <vector>\n";
        code += "#include <cstdlib>\n";
        code += "#include <unistd.h>\n";
        code += "#include <sys/wait.h>\n";
        code += "#include <fcntl.h>\n";
        code += "using namespace std;\n\n";
        code += "int main() {\n";

        // Construct LLDB-DAP command arguments
        code += "    vector<string> lldb_args = {\"lldb-dap\", \"--port\", \"" + m_lldb_port + "\"};\n";
        // Add additional configuration from m_debugger_config
        auto it = m_debugger_config.find("lldb");
        if (it != m_debugger_config.end() && it->is_object())
        {
            if (it->contains("initCommands"))
            {
                std::cout << "Adding init commands to lldb-dap command" << std::endl;
                for (const auto& cmd : it->at("initCommands").get<std::vector<std::string>>())
                {
                    std::cout << "Adding command: " << cmd << std::endl;
                    // Escape quotes in the command for C++ string
                    std::string escaped_cmd = cmd;
                    size_t pos = 0;
                    while ((pos = escaped_cmd.find("\"", pos)) != std::string::npos)
                    {
                        escaped_cmd.replace(pos, 1, "\\\"");
                        pos += 2;
                    }
                    while ((pos = escaped_cmd.find("\\", pos)) != std::string::npos
                           && pos < escaped_cmd.length() - 1)
                    {
                        if (escaped_cmd[pos + 1] != '\"')
                        {
                            escaped_cmd.replace(pos, 1, "\\\\");
                            pos += 2;
                        }
                        else
                        {
                            pos += 2;
                        }
                    }
                    code += "    lldb_args.push_back(\"--init-command\");\n";
                    code += "    lldb_args.push_back(\"" + escaped_cmd + "\");\n";
                }
            }
        }

        // Set up log directory and file
        std::string log_dir = xeus::get_temp_directory_path() + "/xcpp_debug_logs_"
                              + std::to_string(xeus::get_current_pid());
        xeus::create_directory(log_dir);
        std::string log_file = log_dir + "/lldb-dap.log";

        // Add code to start the subprocess with proper redirection
        code += "    string log_file = \"" + log_file + "\";\n";
        code += "    \n";
        code += "    pid_t pid = fork();\n";
        code += "    if (pid == 0) {\n";
        code += "        // Child process - redirect stdout/stderr to log file\n";
        code += "        int fd = open(log_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);\n";
        code += "        if (fd != -1) {\n";
        code += "            dup2(fd, STDOUT_FILENO);\n";
        code += "            dup2(fd, STDERR_FILENO);\n";
        code += "            close(fd);\n";
        code += "        }\n";
        code += "        \n";
        code += "        // Redirect stdin to /dev/null\n";
        code += "        int null_fd = open(\"/dev/null\", O_RDONLY);\n";
        code += "        if (null_fd != -1) {\n";
        code += "            dup2(null_fd, STDIN_FILENO);\n";
        code += "            close(null_fd);\n";
        code += "        }\n";
        code += "        \n";
        code += "        // Convert vector to char* array for execvp\n";
        code += "        vector<char*> args;\n";
        code += "        for (auto& arg : lldb_args) {\n";
        code += "            args.push_back(const_cast<char*>(arg.c_str()));\n";
        code += "        }\n";
        code += "        args.push_back(nullptr);\n";
        code += "        \n";
        code += "        execvp(\"lldb-dap\", args.data());\n";
        code += "        \n";
        code += "        // If execvp fails\n";
        code += "        cerr << \"Failed to execute lldb-dap\" << endl;\n";
        code += "        exit(1);\n";
        code += "    }\n";
        code += "    else if (pid > 0) {\n";
        code += "        // Parent process\n";
        code += "        cout << \"LLDB-DAP process started, PID: \" << pid << endl;\n";
        code += "        \n";
        code += "        // Check if process is still running\n";
        code += "        int status;\n";
        code += "        if (waitpid(pid, &status, WNOHANG) != 0) {\n";
        code += "            cerr << \"LLDB-DAP process exited early\" << endl;\n";
        code += "            return 1;\n";
        code += "        }\n";
        code += "        \n";
        code += "        cout << \"LLDB-DAP started successfully\" << endl;\n";
        code += "    }\n";
        code += "    else {\n";
        code += "        cerr << \"fork() failed\" << endl;\n";
        code += "        return 1;\n";
        code += "    }\n";
        code += "    \n";
        code += "    return 0;\n";
        code += "}\n";

        std::cout << "Starting LLDB-DAP with port: " << m_lldb_port << std::endl;

        // Execute the C++ code via control messenger
        nl::json json_code;
        json_code["code"] = code;
        nl::json rep = xdebugger::get_control_messenger().send_to_shell(json_code);
        std::string status = rep["status"].get<std::string>();

        std::cout << "LLDB-DAP start response: " << rep.dump() << std::endl;

        if (status != "ok")
        {
            std::string ename = rep["ename"].get<std::string>();
            std::string evalue = rep["evalue"].get<std::string>();
            std::vector<std::string> traceback = rep["traceback"].get<std::vector<std::string>>();
            std::clog << "Exception raised when trying to start LLDB-DAP" << std::endl;
            for (std::size_t i = 0; i < traceback.size(); ++i)
            {
                std::clog << traceback[i] << std::endl;
            }
            std::clog << ename << " - " << evalue << std::endl;
            return false;
        }
        else
        {
            std::cout << xcpp::green_text("LLDB-DAP process started successfully") << std::endl;
        }

        m_is_running = true;
        return status == "ok";
    }

    bool debugger::start()
    {
        std::cout << "Starting debugger..." << std::endl;

        // Start LLDB-DAP process
        static bool lldb_started = start_lldb();
        if (!lldb_started)
        {
            std::cout << "Failed to start LLDB-DAP" << std::endl;
            return false;
        }
        // Bind xeus debugger sockets for Jupyter communication
        std::string controller_end_point = xeus::get_controller_end_point("debugger");
        std::string controller_header_end_point = xeus::get_controller_end_point("debugger_header");
        std::string publisher_end_point = xeus::get_publisher_end_point();
        bind_sockets(controller_header_end_point, controller_end_point);

        std::cout << "Debugger sockets bound to: " << controller_end_point << std::endl;
        std::cout << "Debugger header sockets bound to: " << controller_header_end_point << std::endl;
        std::cout << "Publisher sockets bound to: " << publisher_end_point << std::endl;
        std::cout << "LLDB-DAP host: " << m_lldb_host << ", port: " << m_lldb_port << std::endl;

        // Start LLDB-DAP client thread (for ZMQ communication)
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

        // Now test direct TCP communication
        nl::json init_request = {
            {"seq", 1},
            {"type", "request"},
            {"command", "initialize"},
            {"arguments",
             {{"adapterID", "xcpp17"},
              {"clientID", "jupyterlab"},
              {"clientName", "JupyterLab"},
              {"columnsStartAt1", true},
              {"linesStartAt1", true},
              {"locale", "en"},
              {"pathFormat", "path"},
              {"supportsRunInTerminalRequest", true},
              {"supportsVariablePaging", true},
              {"supportsVariableType", true}}}
        };
        // Also test ZMQ path
        send_recv_request("REQ");

        // std::cout << forward_message(init_request).dump() << std::endl;

        // Create temporary folder for cell code
        std::string tmp_folder = get_tmp_prefix();
        xeus::create_directory(tmp_folder);

        return true;
    }

    // Dummy implementations for other methods
    nl::json debugger::inspect_variables_request(const nl::json& message)
    {
        // Placeholder DAP response
        // std::cout << "Sending setBreakpoints request..." << std::endl;
        // nl::json breakpoint_request = {
        //     {"seq", 3},
        //     {"type", "request"},
        //     {"command", "setBreakpoints"},
        //     {"arguments", {
        //         {"source", {
        //             {"name", "input_line_1"},
        //             {"path", "/Users/abhinavkumar/Desktop/Coding/Testing/input_line_1"}
        //         }},
        //         {"breakpoints", {{{"line", 8}}}},
        //         {"lines", {8}},
        //         {"sourceModified", false}
        //     }}
        // };
        // nl::json breakpoint_reply = forward_message(breakpoint_request);
        // std::cout << "Breakpoint reply: " << breakpoint_reply.dump() << std::endl;
        // nl::json config_done_request = {
        //     {"seq", 4},
        //     {"type", "request"},
        //     {"command", "configurationDone"}
        // };
        // nl::json config_reply = forward_message(config_done_request);
        // std::cout << "Configuration done reply: " << config_reply.dump() << std::endl;

        // nl::json run_request = {
        //     {"seq", 5},
        //     {"type", "request"},
        //     {"command", "continue"},
        //     {"arguments", nl::json::object()}
        // };
        // nl::json run_reply = forward_message(run_request);
        // std::cout << "Continue reply: " << run_reply.dump() << std::endl;

        std::cout << "inspect_variables_request not implemented" << std::endl;
        std::cout << message.dump() << std::endl;
        nl::json reply = {
            {"type", "response"},
            {"request_seq", message["seq"]},
            {"success", true},
            {"command", message["command"]},
            {"body",
             {{"variables",
               {{{"name", "a"},
             {"value", "100"},
             {"type", "int"},
             {"evaluateName", "a"},
             {"variablesReference", 0}},
            {{"name", "b"},
             {"value", "1000"},
             {"type", "int"},
             {"evaluateName", "b"},
             {"variablesReference", 0}}}}}}
        };
        return reply;
    }

    nl::json debugger::stack_trace_request(const nl::json& message)
    {
        // Placeholder DAP response
        std::cout << "stack_trace_request not implemented" << std::endl;
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
        std::cout << "debugger::attach_request" << std::endl;
        nl::json attach_request = {
            {"seq", 2},
            {"type", "request"},
            {"command", "attach"},
            {"arguments", {
            {"pid", message["arguments"].value("pid", 0)},
            {"program", message["arguments"].value("program", "")},
            {"stopOnEntry", message["arguments"].value("stopOnEntry", false)},
            {"initCommands", message["arguments"].value("initCommands", nl::json::array())}
            }}
        };
        nl::json reply = forward_message(attach_request);
        std::cout << "Attach request sent: " << reply.dump() << std::endl;
        return reply;
    }

    nl::json debugger::configuration_done_request(const nl::json& message)
    {
        // Minimal DAP response to allow DAP workflow to proceed
        std::cout << "configuration_done_request not implemented" << std::endl;
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
        std::cout << "variables_request_impl not implemented" << std::endl;
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
        std::cout << "get_debugger_info called" << std::endl;
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
        std::cout << "get_cell_temporary_file called" << std::endl;
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
        std::cout << "Debugger config: " << debugger_config.dump() << std::endl;
        std::cout << "User name: " << user_name << std::endl;
        std::cout << "Session ID: " << session_id << std::endl;
        // std::cout << "Context: " << context.get_context_id() << std::endl;
        // std::cout << "Config: " << config.dump() << std::endl;
        return std::unique_ptr<xeus::xdebugger>(
            new debugger(context, config, user_name, session_id, debugger_config)
        );
    }
}