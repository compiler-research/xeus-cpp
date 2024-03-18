/***************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors
 *
 * Distributed under the terms of the BSD 3-Clause License.
 *
 * The full license is in the file LICENSE, distributed with this software.
 ****************************************************************************/

#include "doctest/doctest.h"
#include "xeus-cpp/xinterpreter.hpp"
#include "xeus-cpp/xutils.hpp"
#include "../src/xparser.hpp"

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

TEST_SUITE("extract_filename")
{
    TEST_CASE("extract_filename_basic_test")
    {
        const char* arguments[] = {"argument1", "-f", "filename.txt", "argument4"};
        int argc = sizeof(arguments) / sizeof(arguments[0]);

        std::string result = xcpp::extract_filename(argc, const_cast<char**>(arguments));
        REQUIRE(result == "filename.txt");
        REQUIRE(argc == 2);
    }
}

TEST_SUITE("trim"){

    TEST_CASE("trim_basic_test"){
        std::string argument = "argument";

        std::string result = xcpp::trim(argument);

        REQUIRE(result == "argument");
    }

    /*Checks if it trims the string which 
    has an empty space at the start and in the end*/
    TEST_CASE("trim_start_and_end"){
        std::string argument = " argument ";

        std::string result = xcpp::trim(argument);

        REQUIRE(result == "argument");
    }

    /*Checks if it trims the string which has no characters*/
    TEST_CASE("trim_start_and_end"){
        std::string argument = "  ";

        std::string result = xcpp::trim(argument);

        REQUIRE(result == "");
    }

}