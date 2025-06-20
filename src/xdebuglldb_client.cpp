#include "xdebuglldb_client.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include "nlohmann/json.hpp"
#include "xeus/xmessage.hpp"
#include <iostream>

namespace nl = nlohmann;

namespace xcpp
{
    xdebuglldb_client::xdebuglldb_client(
        xeus::xcontext& context,
        const xeus::xconfiguration& config,
        int socket_linger,
        const xdap_tcp_configuration& dap_config,
        const event_callback& cb
    )
        : base_type(context, config, socket_linger, dap_config, cb)
    {
        std::cout << "xdebuglldb_client initialized" << std::endl;
    }


    void xdebuglldb_client::handle_event(nl::json message)
    {
        // Forward DAP events to the base class (e.g., "stopped" events from LLDB-DAP)
        forward_event(std::move(message));
    }

    nl::json xdebuglldb_client::get_stack_frames(int thread_id, int seq)
    {
        // Construct a DAP stackTrace request
        nl::json request = {
            {"type", "request"},
            {"seq", seq},
            {"command", "stackTrace"},
            {"arguments", {{"threadId", thread_id}}}
        };

        // Send the request
        send_dap_request(std::move(request));

        // Wait for the response
        nl::json reply = wait_for_message(
            [](const nl::json& message)
            {
                return message["type"] == "response" && message["command"] == "stackTrace";
            }
        );

        return reply["body"]["stackFrames"];
    }
}