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
        nl::json result = interpreter.execute_request(code, false, false, user_expressions, false);
        REQUIRE(result["payload"][0]["data"]["text/plain"] == inspect_result);
        REQUIRE(result["user_expressions"] == nl::json::object());
        REQUIRE(result["found"] == true);
        REQUIRE(result["status"] == "ok");
    }
    TEST_CASE("code_execution_failure")
    {
        xcpp::interpreter interpreter(0, nullptr);
        std::string code = "int x = 1 / 0;";  // Divide by zero error
        nl::json user_expressions = nl::json::object();
        nl::json result = interpreter.execute_request(code, false, false, user_expressions, false);
        REQUIRE(result["status"] == "error");
    }
    TEST_CASE("silent_mode")
    {
        xcpp::interpreter interpreter(0, nullptr);
        std::string code = "std::cout << \"Hello, world\";";
        nl::json user_expressions = nl::json::object();
        nl::json result = interpreter.execute_request(
            code,
            true,  // Silent mode
            false,
            user_expressions,
            false
        );
        REQUIRE(result["status"] == "ok");
        REQUIRE(result["payload"].is_null());  // No output payload
    }
    TEST_CASE("user_expressions")
    {
        xcpp::interpreter interpreter(0, nullptr);
        std::string code = "int x = 42;";
        nl::json user_expressions = {{"x_squared", "x * x"}};
        nl::json result = interpreter.execute_request(code, false, false, user_expressions, false);
        REQUIRE(result["user_expressions"]["x_squared"] == 1764);  // 42 * 42
    }
}

// TEST_SUITE("extract_filename")
// {
//     TEST_CASE("extract_filename_basic_test")
//     {
//         const char* arguments[] = {"argument1", "-f", "filename.txt", "argument4"};
//         int argc = sizeof(arguments) / sizeof(arguments[0]);

//         std::string result = xcpp::extract_filename(argc, const_cast<char**>(arguments));
//         REQUIRE(result == "filename.txt");
//         REQUIRE(argc == 2);
//     }
// }
TEST_SUITE("should_print_version_tests")
{
    TEST_CASE("returns_true_when_version_argument_is_provided")
    {
        char* argv[] = {"program", "--version"};
        int argc = sizeof(argv) / sizeof(argv[0]);
        bool res = xcpp::should_print_version(argc, argv) 
        REQUIRE(res == true);
    }

    TEST_CASE("returns_false_when_version_argument_is_not_provided")
    {
        char* argv[] = {"program", "--help"};
        int argc = sizeof(argv) / sizeof(argv[0]);
        bool res = xcpp::should_print_version(argc, argv) 
        REQUIRE(res == false);
    }

    TEST_CASE("returns_true_when_version_argument_is_provided_among_others")
    {
        char* argv[] = {"program", "--help", "--version", "--other"};
        int argc = sizeof(argv) / sizeof(argv[0]);
        bool res = xcpp::should_print_version(argc, argv) 
        REQUIRE(res == true);
    }

    TEST_CASE("returns_false_when_no_arguments_are_provided")
    {
        char* argv[] = {"program"};
        int argc = sizeof(argv) / sizeof(argv[0]);
        bool res = xcpp::should_print_version(argc, argv) 
        REQUIRE(res == false);
    }

    TEST_CASE("returns_false_when_empty_arguments_are_provided")
    {
        char* argv[] = {"program", "", ""};
        int argc = sizeof(argv) / sizeof(argv[0]);
        bool res = xcpp::should_print_version(argc, argv) 
        REQUIRE(res == false);
    }
}

TEST_SUITE("extract_filename_tests")
{
    TEST_CASE("extract_filename_basic_test")
    {
        char* argv[] = {"argument1", "-f", "filename.txt", "argument4"};
        int argc = sizeof(argv) / sizeof(argv[0]);

        std::string result = xcpp::extract_filename(argc, argv);
        REQUIRE(result == "filename.txt");
        REQUIRE(argc == 2);
    }

    TEST_CASE("extracts_filename_when_other_present")
    {
        char* argv[] = {"program", "-f", "myfile.txt", "other"};
        int argc = sizeof(argv) / sizeof(argv[0]);
        std::string result = xcpp::extract_filename(argc,argv);
        REQUIRE(result == "myfile.txt");
        REQUIRE(argc == 2);
        REQUIRE(std::string(argv[1]) == "other");
    }

    TEST_CASE("returns_empty_string_when_filename_not_provided")
    {
        char* argv[] = {"program", "-f"};
        int argc = sizeof(argv) / sizeof(argv[0]);
        std::string result = xcpp::extract_filename(argc,argv);
        REQUIRE(result.empty());
    }

    TEST_CASE("returns_empty_string_when_flag_not_present")
    {
        char* argv[] = {"program", "other", "arguments"};
        int argc = sizeof(argv) / sizeof(argv[0]);
        std::string result = xcpp::extract_filename(argc,argv);
        REQUIRE(result.empty());
        REQUIRE(argc == 3);
    }

    TEST_CASE("handles_multiple_instances_of_flag_correctly")
    {
        char* argv[] = {"program", "-f", "firstfile.txt", "-f", "secondfile.txt", "other"};
        int argc = sizeof(argv) / sizeof(argv[0]);
        std::string result = xcpp::extract_filename(argc, argv);
        REQUIRE(result == "firstfile.txt");
        REQUIRE(argc == 4);  // Ensures only the first -f and its filename are removed
        REQUIRE(std::string(argv[1]) == "-f");
        REQUIRE(std::string(argv[2]) == "secondfile.txt");
    }

    TEST_CASE("properly_shifts_arguments_when_filename_extracted")
    {
        char* argv[] = {"program", "-f", "myfile.txt", "arg1", "arg2"};
        int argc = sizeof(argv) / sizeof(argv[0]);
        std::string result = xcpp::extract_filename(argc, argv);
        REQUIRE(result == "myfile.txt");
        REQUIRE(argc == 3);
        REQUIRE(std::string(argv[1]) == "arg1");
        REQUIRE(std::string(argv[2]) == "arg2");
    }
}

TEST_SUITE("build_interpreter_tests")
{
    TEST_CASE("builds_interpreter_with_correct_arguments")
    {
        char* argv[] = {"program", "--arg1", "value1", "-f", "file.txt"};
        int argc = sizeof(argv) / sizeof(argv[0]);
        auto interp_ptr = xcpp::build_interpreter(argc, argv);

        REQUIRE(interp_ptr != nullptr);
    }

    TEST_CASE("builds_interpreter_with_only_process_name")
    {
        char* argv[] = {"program"};
        int argc = sizeof(argv) / sizeof(argv[0]);
        auto interp_ptr = xcpp::build_interpreter(argc, argv);

        REQUIRE(interp_ptr != nullptr);
    }

}
