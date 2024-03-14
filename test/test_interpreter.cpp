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
        REQUIRE(argc == 3);
    }
}

TEST_SUITE("should_print_version")
{
    /* Checks when the right argument is passed*/
    TEST_CASE("should_print_version_with_version_arg")
    {
        
        const char* arguments[] = {"argument1", "--version"};
        int argc = sizeof(arguments) / sizeof(arguments[0]);;

        bool result = xcpp::should_print_version(argc, const_cast<char**>(arguments));
        REQUIRE(result);
    }

    /* Checks when the wrong argument is passed*/
    TEST_CASE("should_print_version_without_version_arg")
    {
        const char* arguments[] = {"argument1", "argument2"};
        int argc = sizeof(arguments) / sizeof(arguments[0]);;

        bool result = xcpp::should_print_version(argc, const_cast<char**>(arguments));

        REQUIRE(!result);
    }
}