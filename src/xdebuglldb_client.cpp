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
    }

    bool xdebuglldb_client::test_connection(const std::string& endpoint)
    {
        // Parse endpoint (format: tcp://host:port)
        std::string host = "127.0.0.1";
        std::string port_str = endpoint.substr(endpoint.find_last_of(':') + 1);
        int port;
        try
        {
            port = std::stoi(port_str);
        }
        catch (...)
        {
            return false;  // Invalid port
        }

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
        if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0)
        {
            close(sock);
            return false;
        }

        // Set socket timeout for connection
        struct timeval timeout;
        timeout.tv_sec = 2;  // 2-second timeout
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        // Attempt to connect
        if (connect(sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) != 0)
        {
            close(sock);
            return false;
        }

        // Prepare init_request (JSON string)
        std::string init_request = R"({
        "seq": 1,
        "type": "request",
        "command": "initialize",
        "arguments": {
            "clientID": "manual-client",
            "adapterID": "lldb",
            "linesStartAt1": true,
            "columnsStartAt1": true
        }
    })";

        // Prepare DAP message with Content-Length header
        std::ostringstream message_stream;
        message_stream << "Content-Length: " << init_request.length() << "\r\n\r\n" << init_request;
        std::string message = message_stream.str();

        // Send the init_request
        ssize_t bytes_sent = send(sock, message.c_str(), message.length(), 0);
        if (bytes_sent != static_cast<ssize_t>(message.length()))
        {
            close(sock);
            return false;
        }

        // Try to receive a response
        char buffer[4096];
        ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0)
        {
            buffer[bytes_received] = '\0';  // Null-terminate the received data
            // Basic check: if we received any data, consider the connection successful
            std::cout << "Received response: " << buffer << std::endl;
            close(sock);
            return true;
        }

        // No response or error
        close(sock);
        return false;
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