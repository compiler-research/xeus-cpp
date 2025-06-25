#include "xeus-cpp/xdebugger.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include "nlohmann/json.hpp"
#include "xdebuglldb_client.hpp"
#include "xeus-cpp/xinterpreter.hpp"
#include "xeus-zmq/xmiddleware.hpp"
#include "xeus/xsystem.hpp"
#include "xinternal_utils.hpp"

using namespace std::placeholders;

// ***** ONLY FOR DEBUGGING PURPOSES. NOT TO BE COMMITTED *****
static std::ofstream log_stream("/Users/abhinavkumar/Desktop/Coding/CERN_HSF_COMPILER_RESEARCH/xeus-cpp/build/xeus-cpp-logs.log", std::ios::app);

void log_debug(const std::string& msg) {
    log_stream << "[xeus-cpp] " << msg << std::endl;
    log_stream.flush();  // Ensure immediate write
}
// ******* (END) ONLY FOR DEBUGGING PURPOSES. NOT TO BE COMMITTED *******

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
    {
        // Register request handlers
        register_request_handler(
            "inspectVariables",
            std::bind(&debugger::inspect_variables_request, this, _1),
            false
        );
        register_request_handler("attach", std::bind(&debugger::attach_request, this, _1), true);
        register_request_handler(
            "configurationDone",
            std::bind(&debugger::configuration_done_request, this, _1),
            true
        );
        register_request_handler("richInspectVariables", std::bind(&debugger::rich_inspect_variables_request, this, _1), true);
        register_request_handler("setBreakpoints", std::bind(&debugger::set_breakpoints_request, this, _1), true);
        register_request_handler("dumpCell", std::bind(&debugger::dump_cell_request, this, _1), true);
        register_request_handler("copyToGlobals", std::bind(&debugger::copy_to_globals_request, this, _1), true);
        register_request_handler("stackTrace", std::bind(&debugger::stack_trace_request, this, _1), true);

        m_interpreter = reinterpret_cast<xcpp::interpreter*>(
            debugger_config["interpreter_ptr"].get<std::uintptr_t>()
        );
    }

    debugger::~debugger()
    {
        delete p_debuglldb_client;
        p_debuglldb_client = nullptr;
    }

    bool debugger::start_lldb()
    {
        jit_process_pid = interpreter::get_current_pid();

        m_lldb_port = xeus::find_free_port(100, 9999, 10099);
        if (m_lldb_port.empty())
        {
            std::cerr << "Failed to find a free port for LLDB-DAP" << std::endl;
            return false;
        }

        if (std::getenv("XEUS_LOG"))
        {
            std::ofstream log("xeus.log", std::ios::app);
            log << "===== DEBUGGER CONFIG =====\n";
            log << m_debugger_config.dump(4) << '\n';
        }

        // std::vector<std::string> lldb_args = {"/Users/abhinavkumar/Desktop/Coding/CERN_HSF_COMPILER_RESEARCH/llvm-project-lldb/build/bin/lldb-dap", "--connection", "listen://localhost:" + m_lldb_port};
        std::vector<std::string> lldb_args = {
            "lldb-dap",
            "--port",
            m_lldb_port
        };

        std::string log_dir = xeus::get_temp_directory_path() + "/xcpp_debug_logs_"
                              + std::to_string(xeus::get_current_pid());
        xeus::create_directory(log_dir);
        std::string log_file = log_dir + "/lldb-dap.log";

        // std::cout << "Log file for LLDB-DAP: " << log_file << std::endl;

        pid_t pid = fork();
        if (pid == 0)
        {
            // ***** ONLY FOR DEBUGGING PURPOSES. NOT TO BE COMMITTED *****
            int fd = open(log_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd != -1)
            {
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
            // ******* (END) ONLY FOR DEBUGGING PURPOSES. NOT TO BE COMMITTED ******

            int null_fd = open("/dev/null", O_RDONLY);
            if (null_fd != -1)
            {
                dup2(null_fd, STDIN_FILENO);
                close(null_fd);
            }

            std::vector<char*> argv;
            for (auto& arg : lldb_args)
            {
                argv.push_back(const_cast<char*>(arg.c_str()));
            }
            argv.push_back(nullptr);

            execvp("lldb-dap", argv.data());

            std::cerr << "Failed to execute lldb-dap" << std::endl;
            std::exit(1);
        }
        else if (pid > 0)
        {
            int status;
            if (waitpid(pid, &status, WNOHANG) != 0)
            {
                std::cerr << "LLDB-DAP process exited prematurely." << std::endl;
                return false;
            }
            m_is_running = true;
            m_copy_to_globals_available = true;
            return true;
        }
        else
        {
            std::cerr << "fork() failed" << std::endl;
            return false;
        }
    }

    bool debugger::start()
    {
        static bool lldb_started = start_lldb();
        if (!lldb_started)
        {
            std::cerr << "Failed to start LLDB-DAP" << std::endl;
            return false;
        }

        std::string controller_end_point = xeus::get_controller_end_point("debugger");
        std::string controller_header_end_point = xeus::get_controller_end_point("debugger_header");
        std::string publisher_end_point = xeus::get_publisher_end_point();
        bind_sockets(controller_header_end_point, controller_end_point);

        std::string lldb_endpoint = "tcp://" + m_lldb_host + ":" + m_lldb_port;
        std::thread client(
            &xdap_tcp_client::start_debugger,
            p_debuglldb_client,
            lldb_endpoint,
            publisher_end_point,
            controller_end_point,
            controller_header_end_point
        );
        client.detach();

        send_recv_request("REQ");

        std::string tmp_folder = get_tmp_prefix();
        xeus::create_directory(tmp_folder);

        return true;
    }

    nl::json debugger::attach_request(const nl::json& message)
    {
        // Placeholder DAP response
        nl::json attach_request = {
            {"seq", message["seq"]},
            {"type", "request"},
            {"command", "attach"},
            {"arguments",
             {
             {"pid", jit_process_pid},
             {"initCommands", {"settings set plugin.jit-loader.gdb.enable on"}}
             }
            }
        };
        log_debug("[debugger::attach_request] Attach request sent:\n" + attach_request.dump(4));
        nl::json reply = forward_message(attach_request);
        return reply;
    }

    nl::json debugger::inspect_variables_request(const nl::json& message)
    {
        log_debug("[debugger::inspect_variables_request] Inspect variable not implemented.");
        nl::json inspect_request = {
            {"seq", message["seq"]},
            {"type", "request"},
            {"command", "variables"},
            {"arguments", {{"variablesReference", 0}}}
        };
        nl::json dummy_reply = forward_message(inspect_request);
        return dummy_reply;
    }

    nl::json debugger::set_breakpoints_request(const nl::json& message)
    {
        std::string source = message["arguments"]["source"]["path"].get<std::string>();
        m_breakpoint_list.erase(source);
        nl::json bp_json = message["arguments"]["breakpoints"];
        std::vector<nl::json> bp_list(bp_json.begin(), bp_json.end());
        std::string sequential_source = m_hash_to_sequential[source][0];
        m_breakpoint_list.insert(std::make_pair(std::move(source), std::move(bp_list)));
        nl::json mod_message = message;
        mod_message["arguments"]["source"]["path"] = sequential_source;
        log_debug("[debugger::set_breakpoints_request] Set breakpoints request sent:\n" + mod_message.dump(4));
        nl::json breakpoint_reply = forward_message(mod_message);
        if (breakpoint_reply.contains("body") && breakpoint_reply["body"].contains("breakpoints"))
        {
            for (auto& bp : breakpoint_reply["body"]["breakpoints"])
            {
                if (bp.contains("source") && bp["source"].contains("path"))
                {
                    std::string seq_path = bp["source"]["path"].get<std::string>();
                    if (m_sequential_to_hash.find(seq_path) != m_sequential_to_hash.end())
                    {
                        bp["source"]["path"] = m_sequential_to_hash[seq_path];
                    }
                }
            }
        }
        log_debug("[debugger::set_breakpoints_request] Set breakpoints reply received:\n" + breakpoint_reply.dump(4));
        return breakpoint_reply;
    }

    // nl::json debugger::stack_trace_request(const nl::json& message)
    // {
    //     log_debug("Stack trace request received:\n" + message.dump(4));
    //     nl::json reply = {
    //         {"type", "response"},
    //         {"request_seq", message["seq"]},
    //         {"success", false},
    //         {"command", message["command"]},
    //         {"message", "stackTrace not implemented"},
    //         {"body", {{"stackFrames", {}}}}
    //     };
    //     return reply;
    // }

    nl::json debugger::copy_to_globals_request(const nl::json& message)
    {
        // This request cannot be processed if the version of debugpy is lower than 1.6.5.
        log_debug("[debugger::copy_to_globals_request] Copy to globals request received:\n" + message.dump(4));
        if (!m_copy_to_globals_available)
        {
            nl::json reply = {
                {"type", "response"},
                {"request_seq", message["seq"]},
                {"success", false},
                {"command", message["command"]},
                {"body", "The debugpy version must be greater than or equal 1.6.5 to allow copying a variable to the global scope."}
            };
            return reply;
        }

        std::string src_var_name = message["arguments"]["srcVariableName"].get<std::string>();
        std::string dst_var_name = message["arguments"]["dstVariableName"].get<std::string>();
        int src_frame_id = message["arguments"]["srcFrameId"].get<int>();

        // It basically runs a setExpression in the globals dictionary of Python.
        int seq = message["seq"].get<int>();
        std::string expression = "globals()['" + dst_var_name + "']";
        nl::json request = {
            {"type", "request"},
            {"command", "setExpression"},
            {"seq", seq+1},
            {"arguments", {
                {"expression", expression},
                {"value", src_var_name},
                {"frameId", src_frame_id}
            }}
        };
        return forward_message(request);
    }


    nl::json debugger::configuration_done_request(const nl::json& message)
    {
        log_debug("[debugger::configuration_done_request] Configuration done request received:\n" + message.dump(4));
        nl::json reply = forward_message(message);
        log_debug("[debugger::configuration_done_request] Configuration done reply sent:\n" + reply.dump(4));
        // send continue reauest on thread that is stopped
        auto stopped_threads = base_type::get_stopped_threads();
        nl::json continue_request = {
            {"type", "request"},
            {"command", "continue"},
            {"seq", message["seq"].get<int>() + 2},
            {"arguments", {{"threadId", *stopped_threads.begin()}}}
        };
        log_debug("[debugger::configuration_done_request] Sending continue request:\n" + continue_request.dump(4));
        // // wait for 3 seconds before sending continue request
        // std::this_thread::sleep_for(std::chrono::seconds(3));
        nl::json continue_reply = forward_message(continue_request);
        log_debug("[debugger::configuration_done_request] Continue reply received:\n" + continue_reply.dump(4));
        return reply;
    }

    nl::json debugger::variables_request_impl(const nl::json& message)
    {
        log_debug("[debugger::variables_request_impl] Variables request received:\n" + message.dump(4));
        nl::json reply = forward_message(message);
        return reply;
    }

    nl::json debugger::rich_inspect_variables_request(const nl::json& message)
    {
        log_debug("[debugger::rich_inspect_variables_request] Rich inspect variables request received:\n" + message.dump(4));
        return {};
        // nl::json reply = {
        //     {"type", "response"},
        //     {"request_seq", message["seq"]},
        //     {"success", false},
        //     {"command", message["command"]}
        // };

        // std::string var_name = message["arguments"]["variableName"].get<std::string>();
        // py::str py_var_name = py::str(var_name);
        // bool valid_name = PyUnicode_IsIdentifier(py_var_name.ptr()) == 1;
        // if (!valid_name)
        // {
        //     reply["body"] = {
        //         {"data", {}},
        //         {"metadata", {}}
        //     };
        //     if (var_name == "special variables" || var_name == "function variables")
        //     {
        //         reply["success"] = true;
        //     }
        //     return reply;
        // }

        // std::string var_repr_data = var_name + "_repr_data";
        // std::string var_repr_metadata = var_name + "_repr_metada";

        // if (base_type::get_stopped_threads().empty())
        // {
        //     // The code did not hit a breakpoint, we use the interpreter
        //     // to get the rich representation of the variable
        //     std::string code = "from IPython import get_ipython;";
        //     code += var_repr_data + ',' + var_repr_metadata + "= get_ipython().display_formatter.format(" + var_name + ")";
        //     py::gil_scoped_acquire acquire;
        //     exec(py::str(code));
        // }
        // else
        // {
        //     // The code has stopped on a breakpoint, we use the setExpression request
        //     // to get the rich representation of the variable
        //     std::string code = "get_ipython().display_formatter.format(" + var_name + ")";
        //     int frame_id = message["arguments"]["frameId"].get<int>();
        //     int seq = message["seq"].get<int>();
        //     nl::json request = {
        //         {"type", "request"},
        //         {"command", "evaluate"},
        //         {"seq", seq+1},
        //         {"arguments", {
        //             {"expression", code},
        //             {"frameId", frame_id},
        //             {"context", "clipboard"}
        //         }}
        //     };
        //     nl::json request_reply = forward_message(request);
        //     std::string result = request_reply["body"]["result"];

        //     py::gil_scoped_acquire acquire;
        //     std::string exec_code = var_repr_data + ',' + var_repr_metadata + "= eval(str(" + result + "))";
        //     exec(py::str(exec_code));
        // }

        // py::gil_scoped_acquire acquire;
        // py::object variables = py::globals();
        // py::object repr_data = variables[py::str(var_repr_data)];
        // py::object repr_metadata = variables[py::str(var_repr_metadata)];
        // nl::json body = {
        //     {"data", {}},
        //     {"metadata", {}}
        // };
        // for (const py::handle& key : repr_data)
        // {
        //     std::string data_key = py::str(key);
        //     body["data"][data_key] = repr_data[key];
        //     if (repr_metadata.contains(key))
        //     {
        //         body["metadata"][data_key] = repr_metadata[key];
        //     }
        // }
        // PyDict_DelItem(variables.ptr(), py::str(var_repr_data).ptr());
        // PyDict_DelItem(variables.ptr(), py::str(var_repr_metadata).ptr());
        // reply["body"] = body;
        // reply["success"] = true;
        // return reply;
    }

    nl::json debugger::stack_trace_request(const nl::json& message)
    {
        int requested_thread_id = message["arguments"]["threadId"];

        auto stopped_threads = base_type::get_stopped_threads();
        
        if (stopped_threads.empty()) {
            nl::json reply = forward_message(message);
            return reply;
        }
        
        int actual_thread_id;
        nl::json modified_message = message;
        
        if (requested_thread_id == 1 && !stopped_threads.empty()) {
            actual_thread_id = *stopped_threads.begin();
            modified_message["arguments"]["threadId"] = actual_thread_id;
            std::clog << "XDEBUGGER: Mapping client thread 1 to actual thread " << actual_thread_id << std::endl;
        }
        else if (stopped_threads.find(requested_thread_id) != stopped_threads.end()) {
            actual_thread_id = requested_thread_id;
        }
        else {
            actual_thread_id = *stopped_threads.begin();
            modified_message["arguments"]["threadId"] = actual_thread_id;
            std::clog << "XDEBUGGER: Thread " << requested_thread_id 
                    << " not found in stopped threads, using " << actual_thread_id << std::endl;
        }

        for(auto x : stopped_threads)
        {
            log_debug("Stopped thread ID: " + std::to_string(x));
        }

        log_debug("[debugger::stack_trace_request]:\n" + modified_message.dump(4));

        nl::json reply = forward_message(modified_message);
        
        if (!reply.contains("body") || !reply["body"].contains("stackFrames")) {
            return reply;
        }
        
        auto& stack_frames = reply["body"]["stackFrames"];
        for (auto it = stack_frames.begin(); it != stack_frames.end(); ++it) {
            if (it->contains("source") && (*it)["source"].contains("path") && 
                (*it)["source"]["path"] == "<string>") {
                stack_frames.erase(it);
                break;
            }
        }
        
    #ifdef WIN32
        for (auto& frame : stack_frames) {
            if (frame.contains("source") && frame["source"].contains("path")) {
                std::string path = frame["source"]["path"];
                std::replace(path.begin(), path.end(), '\\', '/');
                frame["source"]["path"] = path;
            }
        }
    #endif
        
        return reply;
    }

    void debugger::stop()
    {
        std::string controller_end_point = xeus::get_controller_end_point("debugger");
        std::string controller_header_end_point = xeus::get_controller_end_point("debugger_header");
        unbind_sockets(controller_header_end_point, controller_end_point);
    }

    xeus::xdebugger_info debugger::get_debugger_info() const
    {
        // Placeholder debugger info
        log_debug("[debugger::get_debugger_info] Returning debugger info");
        return xeus::xdebugger_info(
            xeus::get_tmp_hash_seed(),
            get_tmp_prefix(),
            get_tmp_suffix(),
            true,  // Supports exceptions
            {"C++ Exceptions"},
            true  // Supports breakpoints
        );
    }

    nl::json debugger::dump_cell_request(const nl::json& message)
    {
        log_debug("[debugger::dump_cell_request] Dump cell request received:\n" + message.dump(4));
        std::string code;
        try
        {
            code = message["arguments"]["code"].get<std::string>();
            // std::cout << "Execution Count is: " << m_interpreter->get_execution_count(code)[0] << std::endl;
        }
        catch (nl::json::type_error& e)
        {
            std::clog << e.what() << std::endl;
        }
        catch (...)
        {
            std::clog << "XDEBUGGER: Unknown issue" << std::endl;
        }
        std::string hash_file_name = get_cell_temporary_file(code);
        for(auto& exec_count: m_interpreter->get_execution_count(code))
        {
            m_hash_to_sequential[hash_file_name].push_back("input_line_" + std::to_string(exec_count));
            m_sequential_to_hash["input_line_" + std::to_string(exec_count)] = hash_file_name;
        }

        // std::string next_file_name = "input_line_" + std::to_string(m_interpreter->get_execution_count(code).size() ? m_interpreter->get_execution_count(code)[0] : 0);

        std::fstream fs(hash_file_name, std::ios::in);
        if (!fs.is_open())
        {
            fs.clear();
            fs.open(hash_file_name, std::ios::out);
            fs << code;
        }

        nl::json reply = {
            {"type", "response"},
            {"request_seq", message["seq"]},
            {"success", true},
            {"command", message["command"]},
            {"body", {{"sourcePath", hash_file_name}}}
        };
        return reply;
    }

    std::string debugger::get_cell_temporary_file(const std::string& code) const
    {
        return get_cell_tmp_file(code);
    }

    std::unique_ptr<xeus::xdebugger> make_cpp_debugger(
        xeus::xcontext& context,
        const xeus::xconfiguration& config,
        const std::string& user_name,
        const std::string& session_id,
        const nl::json& debugger_config
    )
    {
        return std::unique_ptr<xeus::xdebugger>(
            new debugger(context, config, user_name, session_id, debugger_config)
        );
    }
}