/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#ifndef XEUS_CPP_DEBUGGER_HPP
#define XEUS_CPP_DEBUGGER_HPP

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

#include <map>
#include <mutex>
#include <set>
#include <string>

#include "nlohmann/json.hpp"
#include "xeus-cpp/xinterpreter.hpp"
#include "xeus-zmq/xdebugger_base.hpp"
#include "xeus_cpp_config.hpp"

namespace xcpp
{
    class xdebuglldb_client;

    class XEUS_CPP_API debugger : public xeus::xdebugger_base
    {
    public:

        using base_type = xeus::xdebugger_base;

        debugger(
            xeus::xcontext& context,
            const xeus::xconfiguration& config,
            const std::string& user_name,
            const std::string& session_id,
            const nl::json& debugger_config
        );

        virtual ~debugger();

    private:

        nl::json inspect_variables_request(const nl::json& message);
        nl::json stack_trace_request(const nl::json& message);
        nl::json attach_request(const nl::json& message);
        nl::json configuration_done_request(const nl::json& message);
        nl::json set_breakpoints_request(const nl::json& message);
        nl::json dump_cell_request(const nl::json& message);
        nl::json rich_inspect_variables_request(const nl::json& message);
        nl::json copy_to_globals_request(const nl::json& message);
        nl::json source_request(const nl::json& message);

        bool get_variable_info_from_lldb(
            const std::string& var_name,
            int frame_id,
            nl::json& data,
            nl::json& metadata,
            int sequence
        );
        void get_container_info(
            const std::string& var_name,
            int frame_id,
            const std::string& var_type,
            nl::json& data,
            nl::json& metadata,
            int sequence,
            int size
        );
        bool is_container_type(const std::string& type) const;

        bool start_lldb();
        bool start() override;
        void stop() override;
        xeus::xdebugger_info get_debugger_info() const override;
        virtual std::string get_cell_temporary_file(const std::string& code) const override;

        bool connect_to_lldb_tcp();
        std::string send_dap_message(const nl::json& message);
        std::string receive_dap_response();

        xdebuglldb_client* p_debuglldb_client;
        std::string m_lldb_host;
        std::string m_lldb_port;
        nl::json m_debugger_config;
        bool m_is_running;
        int m_tcp_socket;
        bool m_tcp_connected;
        pid_t jit_process_pid;

        using breakpoint_list_t = std::map<std::string, std::vector<nl::json>>;
        breakpoint_list_t m_breakpoint_list;

        xcpp::interpreter* m_interpreter;

        std::unordered_map<std::string, std::vector<std::string>> m_hash_to_sequential;
        std::unordered_map<std::string, std::string> m_sequential_to_hash;

        bool m_copy_to_globals_available;
    };

    XEUS_CPP_API
    std::unique_ptr<xeus::xdebugger> make_cpp_debugger(
        xeus::xcontext& context,
        const xeus::xconfiguration& config,
        const std::string& user_name,
        const std::string& session_id,
        const nl::json& debugger_config
    );
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif