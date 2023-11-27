/***************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors
 *
 * Distributed under the terms of the BSD 3-Clause License.
 *
 * The full license is in the file LICENSE, distributed with this software.
 ****************************************************************************/

#include "doctest/doctest.h"
#include "xeus-cpp/xinterpreter.hpp"

TEST_SUITE("execute_request")
{
    TEST_CASE("fetch_documentation")
    {

        xcpp::interpreter interpreter(0, nullptr);

        std::string code = "?std::vector";
        std::string inspect_result = "https://en.cppreference.com/w/cpp/container/vector";
        nl::json user_expressions = nl::json::object();

        nl::json result = interpreter.execute_request(
            code,
            false,
            false,
            user_expressions,
            false
        );

        REQUIRE(result["payload"][0]["data"]["text/plain"] == inspect_result);
        REQUIRE(result["user_expressions"] == nl::json::object());
        REQUIRE(result["found"] == true);
        REQUIRE(result["status"] == "ok");
    }
}