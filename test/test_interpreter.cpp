/***************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors
 *
 * Distributed under the terms of the BSD 3-Clause License.
 *
 * The full license is in the file LICENSE, distributed with this software.
 ****************************************************************************/

#include "doctest/doctest.h"
#include "xeus-cpp/xinterpreter.hpp"
#include "xeus-cpp/xholder.hpp"
#include "xeus-cpp/xmanager.hpp"
#include "xeus-cpp/xutils.hpp"
#include "xeus-cpp/xoptions.hpp"

#include "../src/xparser.hpp"
#include "../src/xsystem.hpp"
#include "../src/xmagics/os.hpp"

#include <iostream>

#include <fstream>
#if defined(__GNUC__) && !defined(XEUS_CPP_EMSCRIPTEN_WASM_BUILD)
    #include <sys/wait.h>
    #include <unistd.h>
#endif


/// A RAII class to redirect a stream to a stringstream.
///
/// This class redirects the output of a given std::ostream to a std::stringstream.
/// The original stream is restored when the object is destroyed.
class StreamRedirectRAII {
    public:

        /// Constructor that starts redirecting the given stream.
        StreamRedirectRAII(std::ostream& stream) : old_stream_buff(stream.rdbuf()), stream_to_redirect(stream) {
            stream_to_redirect.rdbuf(ss.rdbuf());
        }

        /// Destructor that restores the original stream.
        ~StreamRedirectRAII() {
            stream_to_redirect.rdbuf(old_stream_buff);
        }

        /// Get the output that was written to the stream.
        std::string getCaptured() {
            return ss.str();
        }

    private:
        /// The original buffer of the stream.
        std::streambuf* old_stream_buff;

        /// The stream that is being redirected.
        std::ostream& stream_to_redirect;

        /// The stringstream that the stream is redirected to.
        std::stringstream ss;
};

TEST_SUITE("execute_request")
{
    TEST_CASE("stl")
    {
        std::vector<const char*> Args = {"stl-test-case", "-v"};
        xcpp::interpreter interpreter((int)Args.size(), Args.data());
        std::string code = "#include <vector>";
        nl::json user_expressions = nl::json::object();
        nl::json result = interpreter.execute_request(
            code,
            /*silent=*/false,
            /*store_history=*/false,
            user_expressions,
            /*allow_stdin=*/false
        );
        REQUIRE(result["status"] == "ok");
    }

    TEST_CASE("fetch_documentation")
    {
        std::vector<const char*> Args = {/*"-v", "resource-dir", "....."*/};
        xcpp::interpreter interpreter((int)Args.size(), Args.data());

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

    TEST_CASE("bad_status")
    {
        std::vector<const char*> Args = {"resource-dir"};
        xcpp::interpreter interpreter((int)Args.size(), Args.data());

        std::string code = "int x = ;";
        nl::json user_expressions = nl::json::object();

        nl::json result = interpreter.execute_request(
            code, 
            false, 
            false, 
            user_expressions, 
            false
        );

        REQUIRE(result["status"] == "error");
    }
}

TEST_SUITE("inspect_request")
{
    TEST_CASE("good_status")
    {
        std::vector<const char*> Args = {/*"-v", "resource-dir", "....."*/};
        xcpp::interpreter interpreter((int)Args.size(), Args.data());

        std::string code = "std::vector";
        int cursor_pos = 11;

        nl::json result = interpreter.inspect_request(
            code, 
            cursor_pos, 
            /*detail_level=*/0
        );

        REQUIRE(result["user_expressions"] == nl::json::object());
        REQUIRE(result["found"] == true);
        REQUIRE(result["status"] == "ok");
    }

    TEST_CASE("bad_status")
    {
        std::vector<const char*> Args = {/*"-v", "resource-dir", "....."*/};
        xcpp::interpreter interpreter((int)Args.size(), Args.data());

        std::string code = "nonExistentFunction";
        int cursor_pos = 19;

        nl::json result = interpreter.inspect_request(
            code, 
            cursor_pos, 
            /*detail_level=*/0
        );

        REQUIRE(result["found"] == false);
        REQUIRE(result["status"] == "error");
    }
}

TEST_SUITE("kernel_info_request")
{
    TEST_CASE("good_status")
    {
        std::vector<const char*> Args = {/*"-v", "resource-dir", "....."*/};
        xcpp::interpreter interpreter((int)Args.size(), Args.data());

        nl::json result = interpreter.kernel_info_request();

        REQUIRE(result["implementation"] == "xeus-cpp");
        REQUIRE(result["language_info"]["name"] == "C++");
        REQUIRE(result["language_info"]["mimetype"] == "text/x-c++src");
        REQUIRE(result["language_info"]["codemirror_mode"] == "text/x-c++src");
        REQUIRE(result["language_info"]["file_extension"] == ".cpp");
        REQUIRE(result["status"] == "ok");
    }

}

TEST_SUITE("shutdown_request")
{
    TEST_CASE("good_status")
    {
        std::vector<const char*> Args = {/*"-v", "resource-dir", "....."*/};
        xcpp::interpreter interpreter((int)Args.size(), Args.data());

        REQUIRE_NOTHROW(interpreter.shutdown_request());
    }

}

TEST_SUITE("is_complete_request")
{
    TEST_CASE("incomplete_code")
    {
        std::vector<const char*> Args = {/*"-v", "resource-dir", "....."*/};
        xcpp::interpreter interpreter((int)Args.size(), Args.data());

        std::string code = "int main() \\";
        nl::json result = interpreter.is_complete_request(code);
        REQUIRE(result["status"] == "incomplete");
    }

    TEST_CASE("complete_code")
    {
        std::vector<const char*> Args = {/*"-v", "resource-dir", "....."*/};
        xcpp::interpreter interpreter((int)Args.size(), Args.data());

        std::string code = "int main() {}";
        nl::json result = interpreter.is_complete_request(code);
        REQUIRE(result["status"] == "complete");
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
    TEST_CASE("trim_empty"){
        std::string argument = "  ";

        std::string result = xcpp::trim(argument);

        REQUIRE(result == "");
    }

    /*Checks if it trims the string is empty*/
    TEST_CASE("trim_empty"){
        std::string argument = "";

        std::string result = xcpp::trim(argument);

        REQUIRE(result == "");
    }

}

TEST_SUITE("should_print_version")
{
    // This test case checks if the function `should_print_version` correctly identifies
    // when the "--version" argument is passed. It sets up a scenario where "--version"
    // is one of the command line arguments and checks if the function returns true.
    TEST_CASE("should_print_version_with_version_arg")
    {
        char arg1[] = "program_name";
        char arg2[] = "--version";
        char* argv[] = {arg1, arg2};
        int argc = 2;

        bool result = xcpp::should_print_version(argc, argv);

        REQUIRE(result == true);
    }

    // This test case checks if the function `should_print_version` correctly identifies
    // when the "--version" argument is not passed. It sets up a scenario where "--version"
    // is not one of the command line arguments and checks if the function returns false.
    TEST_CASE("should_print_version_without_version_arg")
    {
        char arg1[] = "program_name";
        char arg2[] = "-f";
        char arg3[] = "filename";
        char* argv[] = {arg1, arg2, arg3};
        int argc = 3;

        bool result = xcpp::should_print_version(argc, argv);

        REQUIRE(result == false);
    }
}

TEST_SUITE("build_interpreter")
{
    // This test case checks if the function `build_interpreter` returns a non-null pointer
    // when valid arguments are passed. It sets up a scenario with valid command line arguments
    // and checks if the function returns a non-null pointer.
    TEST_CASE("build_interpreter_pointer_not_null")
    {
        char arg1[] = "program_name";
        char arg2[] = "-option1";
        char* argv[] = {arg1, arg2};
        int argc = 2;

        interpreter_ptr interp_ptr = xcpp::build_interpreter(argc, argv);

        REQUIRE(interp_ptr != nullptr);
    }
}

TEST_SUITE("is_match_magics_manager")
{
    // This test case checks if the function `is_match` correctly identifies strings that match
    // the regex pattern used in `xmagics_manager`. It sets up a scenario where strings that should
    // match the pattern are passed and checks if the function returns true.
    TEST_CASE("is_match_true")
    {
        xcpp::xmagics_manager manager;

        bool result1 = manager.is_match("%%magic");
        bool result2 = manager.is_match("%magic");

        REQUIRE(result1 == true);
        REQUIRE(result2 == true);
    }

    // This test case checks if the function `is_match` correctly identifies strings that do not match
    // the regex pattern used in `xmagics_manager`. It sets up a scenario where strings that should not
    // match the pattern are passed and checks if the function returns false.
    TEST_CASE("is_match_false")
    {
        xcpp::xmagics_manager manager;

        bool result1 = manager.is_match("not a magic");
        bool result2 = manager.is_match("%%");
        bool result3 = manager.is_match("%");

        REQUIRE(result1 == false);
        REQUIRE(result2 == false);
        REQUIRE(result3 == false);
    }
}

TEST_SUITE("clone_magics_manager")
{
    // This test case checks if the function `clone_magics_manager` returns a non-null pointer
    // when called. It doesn't require any specific setup as it's testing the default behavior
    // of the function, and checks if the function returns a non-null pointer.
    TEST_CASE("clone_magics_manager_not_null")
    {
        xcpp::xmagics_manager manager;

        xcpp::xpreamble* clone = manager.clone();

        REQUIRE(clone != nullptr);
    }

    // This test case checks if the function `clone_magics_manager` returns a cloned object
    // of the same type as the original. It calls the function and checks if the type of the
    // returned object matches the type of the original `magics_manager`.
    TEST_CASE("clone_magics_manager_same_type")
    {
        xcpp::xmagics_manager manager;

        xcpp::xpreamble* clone = manager.clone();

        REQUIRE(dynamic_cast<xcpp::xmagics_manager*>(clone) != nullptr);

        delete clone;
    }
}

TEST_SUITE("xpreamble_manager_operator")
{
    // This test case checks if the `xpreamble_manager` correctly registers and accesses
    // a `xpreamble` object. It sets up a scenario where a `xpreamble` object is registered
    // with a name and checks if the object can be accessed using the same name.
    TEST_CASE("register_and_access")
    {
        std::string name = "test";
        xcpp::xpreamble_manager manager;
        xcpp::xmagics_manager* magics = new xcpp::xmagics_manager();
        manager.register_preamble(name, magics);

        xcpp::xholder_preamble& result = manager.operator[](name);

        REQUIRE(&(result.get_cast<xcpp::xmagics_manager>()) == magics);
    }
}

TEST_SUITE("xbuffer")
{
    // This test case checks if the `xoutput_buffer` correctly calls the callback function
    // when the buffer is flushed. It sets up a scenario where a `xoutput_buffer` object is
    // created with a callback function, and checks if the callback function is called when
    // the buffer is flushed.
    TEST_CASE("xoutput_buffer_calls_callback_on_sync")
    {
        std::string callback_output;
        auto callback = [&callback_output](const std::string& value)
        {
            callback_output = value;
        };
        xcpp::xoutput_buffer buffer(callback);
        std::ostream stream(&buffer);

        stream << "Hello, world!";
        stream.flush();

        REQUIRE(callback_output == "Hello, world!");
    }

    // This test case checks if the `xinput_buffer` correctly calls the callback function
    // when the buffer is flushed. It sets up a scenario where a `xinput_buffer` object is
    // created with a callback function, and checks if the callback function is called when
    // the buffer is flushed.
    TEST_CASE("xinput_buffer_calls_callback_on_underflow")
    {
        std::string callback_input;
        auto callback = [&callback_input](std::string& value)
        {
            value = callback_input;
        };
        xcpp::xinput_buffer buffer(callback);
        std::istream stream(&buffer);

        callback_input = "Hello, world!";
        std::string output;
        std::getline(stream, output);

        REQUIRE(output == "Hello, world!");
    }

    // This test case checks if the `xoutput_buffer` correctly handles an empty output.
    // It sets up a scenario where the `xoutput_buffer` is given an empty output, then checks
    // if the buffer correctly identifies and handles this situation without errors or exceptions.
    TEST_CASE("xoutput_buffer_handles_empty_output")
    {
        std::string callback_output;
        auto callback = [&callback_output](const std::string& value)
        {
            callback_output = value;
        };

        xcpp::xoutput_buffer buffer(callback);
        std::ostream stream(&buffer);

        stream << "";
        stream.flush();

        REQUIRE(callback_output == "");
    }

    // This test case checks if the `xinput_buffer` correctly handles an empty input.
    // It sets up a scenario where the `xinput_buffer` is given an empty input, then checks
    // if the buffer correctly identifies and handles this situation without errors or exceptions.
    TEST_CASE("xinput_buffer_handles_empty_input")
    {
        std::string callback_input = "";
        auto callback = [&callback_input](std::string& value)
        {
            value = callback_input;
        };

        xcpp::xinput_buffer buffer(callback);
        std::istream stream(&buffer);

        std::string output;
        std::getline(stream, output);

        REQUIRE(output == "");
    }

    // This test case checks if the `xnull` correctly discards the output.
    // It sets up a scenario where the `xnull` is given some output, then checks
    // if the output is correctly discarded and not stored or returned.
    TEST_CASE("xnull_discards_output")
    {
        xcpp::xnull null_buf;
        std::ostream null_stream(&null_buf);

        null_stream << "Hello, world!";

        REQUIRE(null_stream.good() == true);
    }
}

TEST_SUITE("xoptions")
{
    TEST_CASE("good_status") {
        xcpp::argparser parser("test");
        parser.add_argument("--verbose").help("increase output verbosity").default_value(false).implicit_value(true);
        std::string line = "./main --verbose";

        parser.parse(line);

        REQUIRE(parser["--verbose"] == true);
    } 

    TEST_CASE("bad_status") {
        xcpp::argparser parser("test");
        parser.add_argument("--verbose");
        std::string line = "./main --verbose";

        parser.parse(line);

        bool exceptionThrown = false;
        try {
            bool isVerbose = (parser["--verbose"] == false);
        } catch (const std::exception& e) {
            exceptionThrown = true;
        }
        REQUIRE(exceptionThrown);
    }
}

TEST_SUITE("os")
{
    TEST_CASE("write_new_file") {
        xcpp::writefile wf;
        std::string line = "filename testfile.txt -h";
        std::string cell = "Hello, World!";

        wf(line, cell);

        std::ifstream infile("testfile.txt");
        REQUIRE(infile.good() == true);
        infile.close();

        std::remove("testfile.txt");
    }

    TEST_CASE("overwrite_file") {
        xcpp::writefile wf;
        std::string line = "filename testfile.txt";
        std::string cell = "Hello, World!";

        wf(line, cell);

        std::string overwrite_cell = "Overwrite test";
        
        wf(line, overwrite_cell);

        std::ifstream infile("testfile.txt");
        std::string content;
        std::getline(infile, content);

        REQUIRE(content == overwrite_cell);
        infile.close();

        std::remove("testfile.txt");
    }

    TEST_CASE("append_file") {
        xcpp::writefile wf;
        std::string line = "filename testfile.txt";
        std::string cell = "Hello, World!";

        wf(line, cell);

        std::string append_line = "filename testfile.txt --append";
        std::string append_cell = "Hello, again!";

        wf(append_line, append_cell);

        std::ifstream infile("testfile.txt");
        std::vector<std::string> lines;
        std::string content;
        while(std::getline(infile, content)) {
            lines.push_back(content);
        }
        
        REQUIRE(lines[0] == cell);
        REQUIRE(lines[1] == append_cell);
        infile.close();

        std::remove("testfile.txt");
    }

}

TEST_SUITE("xsystem_clone")
{
    TEST_CASE("clone_xsystem_not_null")
    {
        xcpp::xsystem system;

        xcpp::xpreamble* clone = system.clone();

        REQUIRE(clone != nullptr);
    }

    TEST_CASE("clone_xsystem_same_type")
    {
        xcpp::xsystem system;

        xcpp::xpreamble* clone = system.clone();

        REQUIRE(dynamic_cast<xcpp::xsystem*>(clone) != nullptr);

    }
}

TEST_SUITE("xsystem_apply")
{
    TEST_CASE("apply_xsystem")
    {
        xcpp::xsystem system;
        std::string code = "!echo Hello, World!";
        nl::json kernel_res;

        system.apply(code, kernel_res);

        REQUIRE(kernel_res["status"] == "ok");
    }
}

TEST_SUITE("xmagics_contains"){
    TEST_CASE("bad_status_cell") {
        xcpp::xmagics_manager manager;
        xcpp::xmagic_type magic = xcpp::xmagic_type::cell;
        // manager.register_magic("my_magic", xcpp::xmagic_type::cell);
        
        bool result = manager.contains("my_magic", xcpp::xmagic_type::cell);
        REQUIRE(result == false);
    } 

    TEST_CASE("bad_status_line") {
        xcpp::xmagics_manager manager;
        xcpp::xmagic_type magic = xcpp::xmagic_type::line;
        // manager.register_magic("my_magic", xcpp::xmagic_type::cell);
        
        bool result = manager.contains("my_magic", xcpp::xmagic_type::line);
        REQUIRE(result == false);
    } 
}

class MyMagicLine : public xcpp::xmagic_line {
public:
    virtual void operator()(const std::string& line) override{
        std::cout << line << std::endl;
    }
};

class MyMagicCell : public xcpp::xmagic_cell {
public:
    virtual void operator()(const std::string& line, const std::string& cell) override{
        std::cout << line << cell << std::endl;
    }
};

TEST_SUITE("xmagics_apply"){
    TEST_CASE("bad_status_cell") {
        xcpp::xmagics_manager manager;

        nl::json kernel_res;
        manager.apply("%%dummy", kernel_res);
        REQUIRE(kernel_res["status"] == "error");
    }

    TEST_CASE("bad_status_line") {
        xcpp::xmagics_manager manager;

        nl::json kernel_res;
        manager.apply("%dummy", kernel_res);
        REQUIRE(kernel_res["status"] == "error");
    } 

    TEST_CASE("good_status_line") {

        xcpp::xpreamble_manager preamble_manager;

        preamble_manager.register_preamble("magics", new xcpp::xmagics_manager());

        preamble_manager["magics"].get_cast<xcpp::xmagics_manager>().register_magic("magic2", MyMagicCell());

        nl::json kernel_res;

        preamble_manager["magics"].get_cast<xcpp::xmagics_manager>().apply("%%magic2 qwerty", kernel_res);

        REQUIRE(kernel_res["status"] == "ok");
    } 

    TEST_CASE("good_status_cell") {

        xcpp::xpreamble_manager preamble_manager;

        preamble_manager.register_preamble("magics", new xcpp::xmagics_manager());

        preamble_manager["magics"].get_cast<xcpp::xmagics_manager>().register_magic("magic1", MyMagicLine());

        nl::json kernel_res;

        preamble_manager["magics"].get_cast<xcpp::xmagics_manager>().apply("%magic1 qwerty", kernel_res);

        REQUIRE(kernel_res["status"] == "ok");
    } 

    TEST_CASE("cell magic with empty cell body") {

        xcpp::xmagics_manager manager;

        StreamRedirectRAII redirect(std::cerr);

        manager.apply("test", "line", "");

        REQUIRE(redirect.getCaptured() == "UsageError: %%test is a cell magic, but the cell body is empty.\n"
                    "If you only intend to display %%test help, please use a double line break to fill in the cell body.\n");
    }
}

#if defined(__GNUC__) && !defined(XEUS_CPP_EMSCRIPTEN_WASM_BUILD)
TEST_SUITE("xutils_handler"){
    TEST_CASE("handler") {
        pid_t pid = fork();
        if (pid == 0) {

            signal(SIGSEGV, xcpp::handler);
            exit(0);  

        } else {
            
            int status;
            REQUIRE(WEXITSTATUS(status) == 0);
            
        }
    }
}
#endif

TEST_SUITE("complete_request")
{
    TEST_CASE("completion_test")
    {
        std::vector<const char*> Args = {/*"-v", "resource-dir", "....."*/};
        xcpp::interpreter interpreter((int)Args.size(), Args.data());

        nl::json execute = interpreter.execute_request("#include <iostream>", false, false, nl::json::object(), false);

        REQUIRE(execute["status"] == "ok");

        std::string code = "st";
        int cursor_pos = 2;
        nl::json result = interpreter.complete_request(code, cursor_pos);

        REQUIRE(result["cursor_start"] == 0);
        REQUIRE(result["cursor_end"] == 2);
        REQUIRE(result["status"] == "ok");
        size_t found = 0;
        for (auto& r : result["matches"]) {
            if (r == "static" || r == "struct") {
                found++;
            }
        }
        REQUIRE(found == 2);
    }
}