#ifndef XEUS_CPP_DEBUGLLDB_CLIENT_HPP
#define XEUS_CPP_DEBUGLLDB_CLIENT_HPP

#include "xeus-zmq/xdap_tcp_client.hpp"

namespace xcpp
{
    using xeus::xdap_tcp_client;
    using xeus::xdap_tcp_configuration;

    class xdebuglldb_client : public xdap_tcp_client
    {
    public:

        using base_type = xdap_tcp_client;
        using event_callback = base_type::event_callback;

        xdebuglldb_client(xeus::xcontext& context,
                         const xeus::xconfiguration& config,
                         int socket_linger,
                         const xdap_tcp_configuration& dap_config,
                         const event_callback& cb);

        virtual ~xdebuglldb_client() = default;

    private:

        void handle_event(nl::json message) override;
        nl::json get_stack_frames(int thread_id, int seq);
    };
}

#endif