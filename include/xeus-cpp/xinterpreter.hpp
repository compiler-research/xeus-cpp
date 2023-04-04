/***************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors
 *
 * Distributed under the terms of the BSD 3-Clause License.
 *
 * The full license is in the file LICENSE, distributed with this software.
 ****************************************************************************/


#ifndef XEUS_CPP_INTERPRETER_HPP
#define XEUS_CPP_INTERPRETER_HPP

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

#include <memory>
#include <string>

#include "nlohmann/json.hpp"

#include "xeus/xinterpreter.hpp"
#include "xeus_cpp_config.hpp"


namespace nl = nlohmann;

namespace xcpp
{
    class XEUS_CPP_API interpreter : public xeus::xinterpreter
    {
    public:

        interpreter();
        virtual ~interpreter() = default;

    protected:

        void configure_impl() override;

        nl::json execute_request_impl(
            int execution_counter,
            const std::string& code,
            bool silent,
            bool store_history,
            nl::json user_expressions,
            bool allow_stdin
        ) override;

        nl::json complete_request_impl(const std::string& code, int cursor_pos) override;

        nl::json inspect_request_impl(const std::string& code, int cursor_pos, int detail_level) override;

        nl::json is_complete_request_impl(const std::string& code) override;

        nl::json kernel_info_request_impl() override;

        void shutdown_request_impl() override;
    };
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
