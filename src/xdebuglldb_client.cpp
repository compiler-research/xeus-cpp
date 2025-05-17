#include "xdebuglldb_client.hpp"

#include <chrono>
#include <thread>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "nlohmann/json.hpp"
#include "xeus/xmessage.hpp"

namespace nl = nlohmann;

namespace xcpp
{
    xdebuglldb_client::xdebuglldb_client(xeus::xcontext& context,
                                        const xeus::xconfiguration& config,
                                        int socket_linger,
                                        const xdap_tcp_configuration& dap_config,
                                        const event_callback& cb)
        : base_type(context, config, socket_linger, dap_config, cb)
    {
    }

    bool xdebuglldb_client::test_connection(const std::string& endpoint)
    {
        // Parse endpoint (format: tcp://host:port)
        std::string host = "127.0.0.1";
        std::string port_str = endpoint.substr(endpoint.find_last_of(':') + 1);
        int port = std::stoi(port_str);

        // Create a TCP socket
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            return false;
        }

        // Set up server address
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

        // Attempt to connect with a timeout
        struct timeval timeout;
        timeout.tv_sec = 2; // 2-second timeout
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        int result = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
        close(sock);

        return result == 0;
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
            {"arguments", {
                {"threadId", thread_id}
            }}
        };

        // Send the request
        send_dap_request(std::move(request));

        // Wait for the response
        nl::json reply = wait_for_message([](const nl::json& message)
        {
            return message["type"] == "response" && message["command"] == "stackTrace";
        });

        return reply["body"]["stackFrames"];
    }
}