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
#include <fstream>

namespace nl = nlohmann;

// ***** ONLY FOR DEBUGGING PURPOSES. NOT TO BE COMMITTED *****
static std::ofstream log_stream_client("/Users/abhinavkumar/Desktop/Coding/CERN_HSF_COMPILER_RESEARCH/xeus-cpp/build/xeus-cpp-xdebuglldb_client-logs.log", std::ios::app);

void log_debug_client(const std::string& msg) {
    log_stream_client << "[xeus-cpp-debuglldb_client] " << msg << std::endl;
    log_stream_client.flush();  // Ensure immediate write
}
// ******* (END) ONLY FOR DEBUGGING PURPOSES. NOT TO BE COMMITTED *******

void log_debug(const std::string& msg);


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
        log_debug_client("[xdebuglldb_client::handle_event] Handling event:\n" + message.dump(4));
        forward_event(std::move(message));
    }

    nl::json xdebuglldb_client::get_stack_frames(int thread_id, int seq)
    {
        // Construct a DAP stackTrace request
        log_debug_client("Requesting stack frames for thread ID: " + std::to_string(thread_id) + " with sequence number: " + std::to_string(seq));
        // std::cout << "Requesting stack frames for thread ID: " << thread_id << " with sequence number: " << seq << std::endl;
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