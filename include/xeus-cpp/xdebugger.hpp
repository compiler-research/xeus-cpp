/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 * Copyright (c) 2023, Johan Mabille, Loic Gouarin, Sylvain Corlay, Wolf Vollprecht *
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
#include "xeus-zmq/xdebugger_base.hpp"
#include "xeus_cpp_config.hpp"

namespace xcpp
{
    class xdebuglldb_client;

    class XEUS_CPP_API debugger : public xeus::xdebugger_base
    {
    public:

        using base_type = xeus::xdebugger_base;

        debugger(xeus::xcontext& context,
                 const xeus::xconfiguration& config,
                 const std::string& user_name,
                 const std::string& session_id,
                 const nl::json& debugger_config);

        virtual ~debugger();

    private:

        nl::json inspect_variables_request(const nl::json& message);
        nl::json stack_trace_request(const nl::json& message);
        nl::json attach_request(const nl::json& message);
        nl::json configuration_done_request(const nl::json& message);

        nl::json variables_request_impl(const nl::json& message) override;

        bool start_lldb();
        bool start() override;
        void stop() override;
        xeus::xdebugger_info get_debugger_info() const override;
        std::string get_cell_temporary_file(const std::string& code) const override;

        bool connect_to_lldb_tcp();
        std::string send_dap_message(const nl::json& message);
        std::string receive_dap_response();

        xdebuglldb_client* p_debuglldb_client;
        std::string m_lldb_host;
        std::string m_lldb_port;
        std::string m_lldbdap_port;
        nl::json m_debugger_config;
        bool m_is_running;
        int m_tcp_socket;
        bool m_tcp_connected;
    };

    XEUS_CPP_API
    std::unique_ptr<xeus::xdebugger> make_cpp_debugger(xeus::xcontext& context,
                                                       const xeus::xconfiguration& config,
                                                       const std::string& user_name,
                                                       const std::string& session_id,
                                                       const nl::json& debugger_config);
}

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif

#endif