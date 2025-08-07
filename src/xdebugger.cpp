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
        register_request_handler("source", std::bind(&debugger::source_request, this, _1), true);

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
        bool lldb_started = start_lldb();
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
        if(!base_type::get_stopped_threads().empty()) {
            // Return a dummy reply as per user request.
            nl::json reply = {
                {"type", "response"},
                {"request_seq", message["seq"]},
                {"success", true},
                {"command", message["command"]},
                {"body", {{"info", "Dummy reply: continue request not sent as requested."}}}
            };
            log_debug("[debugger::configuration_done_request] Returning dummy reply instead of sending continue request.");
            return reply;
        }
        nl::json reply = forward_message(message);
        log_debug("[debugger::configuration_done_request] Configuration done reply sent:\n" + reply.dump(4));
        return reply;
    }

    void debugger::get_container_info(const std::string& var_name, int frame_id, 
                             const std::string& var_type, nl::json& data, nl::json& metadata, int sequence, int size)
    {
        // Create a tree structure for the container
        nl::json container_tree = {
            {"type", "container"},
            {"name", var_name},
            {"containerType", var_type},
            {"size", size},
            {"children", nl::json::array()},
            {"expanded", false}
        };
        
        // Add elements as children
        for (int i = 0; i < size; ++i) {
            nl::json elem_request = {
                {"type", "request"},
                {"command", "evaluate"},
                {"seq", sequence++},
                {"arguments", {
                    {"expression", var_name + "[" + std::to_string(i) + "]"},
                    {"frameId", frame_id},
                    {"context", "watch"}
                }}
            };
            
            nl::json elem_reply = forward_message(elem_request);
            if (elem_reply["success"].get<bool>()) {
                std::string elem_value = elem_reply["body"]["result"].get<std::string>();
                std::string elem_type = elem_reply["body"].contains("type") ? 
                                elem_reply["body"]["type"].get<std::string>() : "unknown";
                
                nl::json child_node = {
                    {"type", "element"},
                    {"name", "[" + std::to_string(i) + "]"},
                    {"value", elem_value},
                    {"valueType", elem_type},
                    {"index", i},
                    {"leaf", true}
                };
                
                // Check if element is also a container
                if (is_container_type(elem_type)) {
                    child_node["leaf"] = false;
                    child_node["hasChildren"] = true;
                    // Size extraction for nested containers
                    int nested_size = 0;
                    std::smatch match;
                    std::regex size_regex(R"(size\s*=\s*(\d+))");
                    if (std::regex_search(elem_value, match, size_regex) && match.size() > 1) {
                        nested_size = std::stoi(match[1]);
                    }
                    child_node["size"] = nested_size;
                }
                
                container_tree["children"].push_back(child_node);
            }
        }
        
        // Store the tree structure in JSON format
        data["application/json"] = container_tree;
    }

    bool debugger::is_container_type(const std::string& type) const
    {
        return type.find("std::vector") != std::string::npos ||
            type.find("std::array") != std::string::npos ||
            type.find("std::list") != std::string::npos ||
            type.find("std::deque") != std::string::npos ||
            type.find("std::map") != std::string::npos ||
            type.find("std::set") != std::string::npos ||
            type.find("std::unordered_map") != std::string::npos ||
            type.find("std::unordered_set") != std::string::npos;
    }

    bool debugger::get_variable_info_from_lldb(const std::string& var_name, int frame_id, nl::json& data, nl::json& metadata, int sequence) {
        try {
            nl::json eval_request = {
                {"type", "request"},
                {"command", "evaluate"},
                {"seq", sequence},
                {"arguments", {
                    {"expression", var_name},
                    {"frameId", frame_id},
                    {"context", "watch"}
                }}
            };

            nl::json eval_reply = forward_message(eval_request);
            if (!eval_reply["success"].get<bool>()) {
                return false;
            }
            
            std::string var_value = eval_reply["body"]["result"].get<std::string>();
            std::string var_type = eval_reply["body"].contains("type") ? 
                            eval_reply["body"]["type"].get<std::string>() : "unknown";
            
            // Create a unified JSON structure for all variable types
            nl::json variable_info = {
                {"type", "variable"},
                {"name", var_name},
                {"value", var_value},
                {"valueType", var_type},
                {"frameId", frame_id},
                {"timestamp", std::time(nullptr)}
            };

            if(is_container_type(var_type)) {
                // Extract size from var_value, e.g., "size=2"
                int size = 0;
                std::smatch match;
                std::regex size_regex(R"(size\s*=\s*(\d+))");
                if (std::regex_search(var_value, match, size_regex) && match.size() > 1) {
                    size = std::stoi(match[1]);
                }
                
                variable_info["isContainer"] = true;
                variable_info["size"] = size;
                variable_info["leaf"] = false;
                variable_info["hasChildren"] = size > 0;
                
                // Get detailed container info
                get_container_info(var_name, frame_id, var_type, data, metadata, sequence + 1, size);
                
                // Add container summary to the main variable info
                if (data["application/json"].contains("children")) {
                    variable_info["children"] = data["application/json"]["children"];
                    variable_info["expanded"] = false;
                }
            } else {
                variable_info["isContainer"] = false;
                variable_info["leaf"] = true;
                variable_info["hasChildren"] = false;
            }

            // Store everything in application/json format
            data["application/json"] = variable_info;

            // Metadata for the frontend renderer
            metadata["application/json"] = {
                {"version", "1.0"},
                {"renderer", "variable-inspector"},
                {"expandable", variable_info["hasChildren"]},
                {"rootVariable", var_name},
                {"capabilities", {
                    {"tree", true},
                    {"search", true},
                    {"filter", true},
                    {"export", true}
                }}
            };

            return true;
        } catch (const std::exception& e) {
            log_debug("[get_variable_info_from_lldb] Exception: " + std::string(e.what()));
            return false;
        }
    }

    nl::json debugger::rich_inspect_variables_request(const nl::json& message)
    {
        log_debug("[debugger::rich_inspect_variables_request] Rich inspect variables request received:\n" + message.dump(4));
        nl::json reply = {
            {"type", "response"},
            {"request_seq", message["seq"]},
            {"success", false},
            {"command", message["command"]}
        };

        std::string var_name;
        try {
            var_name = message["arguments"]["variableName"].get<std::string>();
        } catch (const nl::json::exception& e) {
            reply["body"] = {
                {"data", {
                    {"application/json", {
                        {"type", "error"},
                        {"message", "Invalid variable name in request"},
                        {"details", std::string(e.what())}
                    }}
                }},
                {"metadata", {
                    {"application/json", {
                        {"error", true},
                        {"errorType", "invalid_request"}
                    }}
                }}
            };
            return reply;
        }

        int frame_id = 0;
        if(message["arguments"].contains("frameId")) {
            frame_id = message["arguments"]["frameId"].get<int>();
        }

        nl::json var_data, var_metadata;
        bool success = false;

        auto stopped_threads = base_type::get_stopped_threads();
        if(!stopped_threads.empty()) {
            success = get_variable_info_from_lldb(var_name, frame_id, var_data, var_metadata, message["seq"].get<int>() + 1);
        } else {
            // When there is no breakpoint hit, return a placeholder structure
            var_data["application/json"] = {
                {"type", "unavailable"},
                {"name", var_name},
                {"message", "Variable not accessible - no active debugging session"},
                {"suggestions", {
                    "Set a breakpoint and run the program",
                    "Ensure the variable is in scope",
                    "Check if the program is currently stopped"
                }}
            };
            var_metadata["application/json"] = {
                {"available", false},
                {"reason", "no_stopped_threads"}
            };
            success = true;
        }

        if(success) {
            reply["body"] = {
                {"data", var_data},
                {"metadata", var_metadata}
            };
            reply["success"] = true;
        } else {
            reply["body"] = {
                {"data", {
                    {"application/json", {
                        {"type", "error"},
                        {"name", var_name},
                        {"message", "Variable '" + var_name + "' not found or not accessible"},
                        {"frameId", frame_id},
                        {"suggestions", {
                            "Check variable name spelling",
                            "Ensure variable is in current scope",
                            "Verify the frame ID is correct"
                        }}
                    }}
                }},
                {"metadata", {
                    {"application/json", {
                        {"error", true},
                        {"errorType", "variable_not_found"}
                    }}
                }}
            };
        }

        log_debug("[debugger::rich_inspect_variables_request] Reply:\n" + reply.dump(4));
        return reply;
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

        for (auto x : stopped_threads)
        {
            log_debug("Stopped thread ID: " + std::to_string(x));
        }

        log_debug("[debugger::stack_trace_request]:\n" + modified_message.dump(4));

        nl::json reply = forward_message(modified_message);

        if (!reply.contains("body") || !reply["body"].contains("stackFrames")) {
            return reply;
        }

        auto& stack_frames = reply["body"]["stackFrames"];
        nl::json filtered_frames = nl::json::array();

        for (auto& frame : stack_frames)
        {
            if (frame.contains("source") && frame["source"].contains("path"))
            {
                std::string path = frame["source"]["path"];
                std::string name = frame["source"]["name"];
                // Check if path is in the format input_line_<number>
                if (name.find("input_line_") != std::string::npos)
                {
                    // Map sequential filename to hash if mapping exists
                    auto it = m_sequential_to_hash.find(name);
                    if (it != m_sequential_to_hash.end())
                    {
                        frame["source"]["path"] = it->second;
                        // Set name to last part of path (filename)
                        std::string filename = it->second.substr(it->second.find_last_of("/\\") + 1);
                        frame["source"]["name"] = filename;
                    } else {
                        std::string code, hash_file_name;
                        // Extract execution count number from "input_line_<number>"
                        int exec_count = -1;
                        std::string prefix = "input_line_";
                        if (name.compare(0, prefix.size(), prefix) == 0)
                        {
                            try
                            {
                                exec_count = std::stoi(name.substr(prefix.size()));
                            }
                            catch (...)
                            {
                                exec_count = 0;
                            }
                        }
                        
                        if (exec_count != -1) {
                            code = m_interpreter->get_code_from_execution_count(exec_count);
                            hash_file_name = get_cell_temporary_file(code);
                            frame["source"]["path"] = hash_file_name;
                            frame["source"]["name"] = hash_file_name.substr(hash_file_name.find_last_of("/\\") + 1);
                            // Update mappings if not already present
                            m_hash_to_sequential[hash_file_name].push_back(name);
                            m_sequential_to_hash[name] = hash_file_name;
                        }
                    }
                    filtered_frames.push_back(frame);
                }
            }
        }

        reply["body"]["stackFrames"] = filtered_frames;

    #ifdef WIN32
        for (auto& frame : reply["body"]["stackFrames"]) {
            if (frame.contains("source") && frame["source"].contains("path")) {
                std::string path = frame["source"]["path"];
                std::replace(path.begin(), path.end(), '\\', '/');
                frame["source"]["path"] = path;
            }
        }
    #endif

        log_debug("Stack trace response: " + reply.dump(4));

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

    nl::json debugger::source_request(const nl::json& message)
    {
        std::string sourcePath;
        try
        {
            sourcePath = message["arguments"]["source"]["path"].get<std::string>();
        }
        catch(nl::json::type_error& e)
        {
            std::clog << e.what() << std::endl;
        }
        catch(...)
        {
            std::clog << "XDEBUGGER: Unknown issue" << std::endl;
        }

        std::ifstream ifs(sourcePath, std::ios::in);
        if(!ifs.is_open())
        {
            nl::json reply = {
                {"type", "response"},
                {"request_seq", message["seq"]},
                {"success", false},
                {"command", message["command"]},
                {"message", "source unavailable"},
                {"body", {{}}}
            };
            return reply;
        }

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        nl::json reply = {
            {"type", "response"},
            {"request_seq", message["seq"]},
            {"success", true},
            {"command", message["command"]},
            {"body", {
                {"content", content}
            }}
        };
        log_debug("[debugger::source_request reply]:\n" + reply.dump(4));
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