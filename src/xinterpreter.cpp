/***************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors
 *
 * Distributed under the terms of the BSD 3-Clause License.
 *
 * The full license is in the file LICENSE, distributed with this software.
 ****************************************************************************/

#include <iostream>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "xeus/xhelper.hpp"
#include "xeus/xinput.hpp"
#include "xeus/xinterpreter.hpp"

#include "xeus-cpp/xinterpreter.hpp"

namespace nl = nlohmann;

namespace xcpp
{

    interpreter::interpreter()
    {
        xeus::register_interpreter(this);
    }

    nl::json interpreter::execute_request_impl(
        int execution_counter,    // Typically the cell number
        const std::string& code,  // Code to execute
        bool /*silent*/,
        bool /*store_history*/,
        nl::json /*user_expressions*/,
        bool /*allow_stdin*/
    )
    {
        // Use this method for publishing the execution result to the client,
        // this method takes the ``execution_counter`` as first argument,
        // the data to publish (mime type data) as second argument and metadata
        // as third argument.
        // Replace "Hello World !!" by what you want to be displayed under the execution cell
        nl::json pub_data;
        pub_data["text/plain"] = "Hello World !!";

        // If silent is set to true, do not publish anything!
        // Otherwise:
        // Publish the execution result
        publish_execution_result(execution_counter, std::move(pub_data), nl::json::object());

        // You can also use this method for publishing errors to the client, if the code
        // failed to execute
        // publish_execution_error(error_name, error_value, error_traceback);
        publish_execution_error("TypeError", "123", {"!@#$", "*(*"});

        // Use publish_stream to publish a stream message or error:
        publish_stream("stdout", "I am publishing a message");
        publish_stream("stderr", "Error!");

        // Use Helpers that create replies to the server to be returned
        return xeus::create_successful_reply(/*payload, user_expressions*/);
        // Or in case of error:
        // return xeus::create_error_reply(evalue, ename, trace_back);
    }

    void interpreter::configure_impl()
    {
        // `configure_impl` allows you to perform some operations
        // after the custom_interpreter creation and before executing any request.
        // This is optional, but can be useful;
        // you can for example initialize an engine here or redirect output.
    }

    nl::json interpreter::is_complete_request_impl(const std::string& code)
    {
        // Insert code here to validate the ``code``
        // and use `create_is_complete_reply` with the corresponding status
        // "unknown", "incomplete", "invalid", "complete"
        return xeus::create_is_complete_reply("complete" /*status*/, "   " /*indent*/);
    }

    nl::json interpreter::complete_request_impl(const std::string& code, int cursor_pos)
    {
        // Should be replaced with code performing the completion
        // and use the returned `matches` to `create_complete_reply`
        // i.e if the code starts with 'H', it could be the following completion
        if (code[0] == 'H')
        {
            return xeus::create_complete_reply(
                {std::string("Hello"), std::string("Hey"), std::string("Howdy")}, /*matches*/
                5,                                                                /*cursor_start*/
                cursor_pos                                                        /*cursor_end*/
            );
        }

        // No completion result
        else
        {
            return xeus::create_complete_reply(
                nl::json::array(), /*matches*/
                cursor_pos,        /*cursor_start*/
                cursor_pos         /*cursor_end*/
            );
        }
    }

    nl::json
    interpreter::inspect_request_impl(const std::string& /*code*/, int /*cursor_pos*/, int /*detail_level*/)
    {
        return xeus::create_inspect_reply(
            true /*found*/,
            {{std::string("text/plain"), std::string("hello!")}}, /*data*/
            {{std::string("text/plain"), std::string("hello!")}}  /*meta-data*/
        );
    }

    void interpreter::shutdown_request_impl()
    {
        std::cout << "Bye!!" << std::endl;
    }

    nl::json interpreter::kernel_info_request_impl()
    {
        const std::string protocol_version = "5.3";
        const std::string implementation = "xcpp";
        const std::string implementation_version = XEUS_CPP_VERSION;
        const std::string language_name = "cpp";
        const std::string language_version = "14";
        const std::string language_mimetype = "x-c++src";
        ;
        const std::string language_file_extension = "cpp";
        ;
        const std::string language_pygments_lexer = "";
        const std::string language_codemirror_mode = "";
        const std::string language_nbconvert_exporter = "";
        const std::string banner = "xcpp";
        const bool debugger = false;

        const nl::json help_links = nl::json::array();


        return xeus::create_info_reply(
            protocol_version,
            implementation,
            implementation_version,
            language_name,
            language_version,
            language_mimetype,
            language_file_extension,
            language_pygments_lexer,
            language_codemirror_mode,
            language_nbconvert_exporter,
            banner,
            debugger,
            help_links
        );
    }

}
