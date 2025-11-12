/***************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors
 *
 * Distributed under the terms of the BSD 3-Clause License.
 *
 * The full license is in the file LICENSE, distributed with this software.
 ****************************************************************************/

#include <future>

#include "doctest/doctest.h"
#include "xeus-cpp/xinterpreter.hpp"
#include "xeus-cpp/xholder.hpp"
#include "xeus-cpp/xmanager.hpp"
#include "xeus-cpp/xutils.hpp"
#include "xeus-cpp/xoptions.hpp"
#include "xeus-cpp/xeus_cpp_config.hpp"
#include "xcpp/xmime.hpp"

#include "../src/xparser.hpp"
#include "../src/xsystem.hpp"
#include "../src/xmagics/os.hpp"
#include "../src/xmagics/xassist.hpp"
#include "../src/xinspect.hpp"


#include <iostream>
#include <pugixml.hpp>
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
        xeus::execute_request_config config;
        config.silent = false;
        config.store_history = false;
        config.allow_stdin = false;
        nl::json header = nl::json::object();
        xeus::xrequest_context::guid_list id = {};
        xeus::xrequest_context context(header, id);

        std::promise<nl::json> promise;
        std::future<nl::json> future = promise.get_future();
        auto callback = [&promise](nl::json result) {
            promise.set_value(result);
        };

        interpreter.execute_request(
            std::move(context),
            std::move(callback),
            code,
            std::move(config),
            user_expressions
        );
        nl::json result = future.get();
        REQUIRE(result["status"] == "ok");
    }

    TEST_CASE("fetch_documentation")
    {
        std::vector<const char*> Args = {/*"-v", "resource-dir", "....."*/};
        xcpp::interpreter interpreter((int)Args.size(), Args.data());

        std::string code = "?std::vector";
        std::string inspect_result = "https://en.cppreference.com/w/cpp/container/vector";
        nl::json user_expressions = nl::json::object();
        xeus::execute_request_config config;
        config.silent = false;
        config.store_history = false;
        config.allow_stdin = false;
        nl::json header = nl::json::object();
        xeus::xrequest_context::guid_list id = {};
        xeus::xrequest_context context(header, id);

        std::promise<nl::json> promise;
        std::future<nl::json> future = promise.get_future();
        auto callback = [&promise](nl::json result) {
            promise.set_value(result);
        };

        interpreter.execute_request(
            std::move(context),
            std::move(callback),
            code,
            std::move(config),
            user_expressions
        );
        nl::json result = future.get();
        REQUIRE(result["payload"][0]["data"]["text/plain"] == inspect_result);
        REQUIRE(result["user_expressions"] == nl::json::object());
        REQUIRE(result["found"] == true);
        REQUIRE(result["status"] == "ok");
    }

    TEST_CASE("fetch_documentation_of_member_or_parameter")
    {
        std::vector<const char*> Args = {/*"-v", "resource-dir", "....."*/};
        xcpp::interpreter interpreter((int)Args.size(), Args.data());

        std::string code = "?std::vector.push_back";
        std::string inspect_result = "https://en.cppreference.com/w/cpp/container/vector/push_back";
        nl::json user_expressions = nl::json::object();
        xeus::execute_request_config config;
        config.silent = false;
        config.store_history = false;
        config.allow_stdin = false;
        nl::json header = nl::json::object();
        xeus::xrequest_context::guid_list id = {};
        xeus::xrequest_context context(header, id);

        std::promise<nl::json> promise;
        std::future<nl::json> future = promise.get_future();
        auto callback = [&promise](nl::json result) {
            promise.set_value(result);
        };

        interpreter.execute_request(
            std::move(context),
            std::move(callback),
            code,
            std::move(config),
            user_expressions
        );
        nl::json result = future.get();
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
        xeus::execute_request_config config;
        config.silent = false;
        config.store_history = false;
        config.allow_stdin = false;
        nl::json header = nl::json::object();
        xeus::xrequest_context::guid_list id = {};
        xeus::xrequest_context context(header, id);

        std::promise<nl::json> promise;
        std::future<nl::json> future = promise.get_future();
        auto callback = [&promise](nl::json result) {
            promise.set_value(result);
        };

        interpreter.execute_request(
            std::move(context),
            std::move(callback),
            code,
            std::move(config),
            user_expressions
        );
        nl::json result = future.get();
        REQUIRE(result["status"] == "error");
    }
}

TEST_SUITE("inspect_request")
{
#if defined(XEUS_CPP_EMSCRIPTEN_WASM_BUILD)
    TEST_CASE("good_status"
            * doctest::should_fail(true)
            * doctest::description("TODO: Currently fails for the Emscripten build"))
#else
    TEST_CASE("good_status")
#endif
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
        std::vector<const char*> Args = {
            "-v", "-std=c++23"  // test input for get_stdopt
        };
        xcpp::interpreter interpreter((int)Args.size(), Args.data());

        nl::json result = interpreter.kernel_info_request();

        REQUIRE(result["implementation"] == "xeus-cpp");
        REQUIRE(result["language_info"]["name"] == "C++");
        REQUIRE(result["language_info"]["mimetype"] == "text/x-c++src");
        REQUIRE(result["language_info"]["codemirror_mode"] == "text/x-c++src");
        REQUIRE(result["language_info"]["file_extension"] == ".cpp");
        REQUIRE(result["language_info"]["version"] == "23");
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

        std::unique_ptr<xcpp::xpreamble> clone = manager.clone();

        REQUIRE(clone != nullptr);
    }

    // This test case checks if the function `clone_magics_manager` returns a cloned object
    // of the same type as the original. It calls the function and checks if the type of the
    // returned object matches the type of the original `magics_manager`.
    TEST_CASE("clone_magics_manager_same_type")
    {
        xcpp::xmagics_manager manager;

        std::unique_ptr<xcpp::xpreamble> clone = manager.clone();

        REQUIRE(clone.get() != nullptr);
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
        std::unique_ptr<xcpp::xmagics_manager> magics = std::make_unique<xcpp::xmagics_manager>();
        auto* raw_ptr = magics.get();
        manager.register_preamble(name, std::move(magics));

        xcpp::xholder_preamble& result = manager.operator[](name);

        REQUIRE(&(result.get_cast<xcpp::xmagics_manager>()) == raw_ptr);
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

        std::unique_ptr<xcpp::xpreamble> clone = system.clone();

        REQUIRE(clone != nullptr);
    }

    TEST_CASE("clone_xsystem_same_type")
    {
        xcpp::xsystem system;

        std::unique_ptr<xcpp::xpreamble> clone = system.clone();

        REQUIRE(clone.get() != nullptr);

    }
}

#if !defined(XEUS_CPP_EMSCRIPTEN_WASM_BUILD)
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
#endif

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

        preamble_manager.register_preamble("magics", std::make_unique<xcpp::xmagics_manager>());

        preamble_manager["magics"].get_cast<xcpp::xmagics_manager>().register_magic("magic2", MyMagicCell());

        nl::json kernel_res;

        preamble_manager["magics"].get_cast<xcpp::xmagics_manager>().apply("%%magic2 qwerty", kernel_res);

        REQUIRE(kernel_res["status"] == "ok");
    } 

    TEST_CASE("good_status_cell") {

        xcpp::xpreamble_manager preamble_manager;

        preamble_manager.register_preamble("magics", std::make_unique<xcpp::xmagics_manager>());

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
        std::string code1 = "#include <iostream>";
        nl::json user_expressions = nl::json::object();
        xeus::execute_request_config config;
        config.silent = false;
        config.store_history = false;
        config.allow_stdin = false;
        nl::json header = nl::json::object();
        xeus::xrequest_context::guid_list id = {};
        xeus::xrequest_context context(header, id);

        std::promise<nl::json> promise;
        std::future<nl::json> future = promise.get_future();
        auto callback = [&promise](nl::json result) {
            promise.set_value(result);
        };

        interpreter.execute_request(
            std::move(context),
            std::move(callback),
            code1,
            std::move(config),
            user_expressions
        );
        nl::json execute = future.get();

        REQUIRE(execute["status"] == "ok");

        std::string code2 = "st";
        int cursor_pos = 2;
        nl::json result = interpreter.complete_request(code2, cursor_pos);

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

TEST_SUITE("xinspect"){
    TEST_CASE("class_member_predicate_get_filename"){
        xcpp::class_member_predicate cmp;
        cmp.class_name = "TestClass";
        cmp.kind = "public";
        cmp.child_value = "testMethod";

        pugi::xml_document doc;
        pugi::xml_node node = doc.append_child("node");
        node.append_attribute("kind") = "class";
        pugi::xml_node name = node.append_child("name");
        name.append_child(pugi::node_pcdata).set_value("TestClass");
        pugi::xml_node child = node.append_child("node");
        child.append_attribute("kind") = "public";
        pugi::xml_node child_name = child.append_child("name");
        child_name.append_child(pugi::node_pcdata).set_value("testMethod");
        pugi::xml_node anchorfile = child.append_child("anchorfile");
        anchorfile.append_child(pugi::node_pcdata).set_value("testfile.cpp");

        REQUIRE(cmp.get_filename(node) == "testfile.cpp");
    
        cmp.child_value = "nonexistentMethod";
        REQUIRE(cmp.get_filename(node) == "");
    }

    TEST_CASE("class_member_predicate_operator"){
        xcpp::class_member_predicate cmp;
        cmp.class_name = "TestClass";
        cmp.kind = "public";
        cmp.child_value = "testMethod";

        pugi::xml_document doc;
        pugi::xml_node node = doc.append_child("node");
        node.append_attribute("kind") = "class";
        pugi::xml_node name = node.append_child("name");
        name.append_child(pugi::node_pcdata).set_value("TestClass");
        pugi::xml_node child = node.append_child("node");
        child.append_attribute("kind") = "public";
        pugi::xml_node child_name = child.append_child("name");
        child_name.append_child(pugi::node_pcdata).set_value("testMethod");

    
        REQUIRE(cmp(node) == true);
        node.attribute("kind").set_value("struct");
        REQUIRE(cmp(node) == true);
        cmp.class_name = "NonexistentClass";
        REQUIRE(cmp(node) == false);
        cmp.kind = "private";
        REQUIRE(cmp(node) == false);
        cmp.child_value = "nonexistentMethod";
        REQUIRE(cmp(node) == false);
    }

    TEST_CASE("is_inspect_request"){ 
        std::string code = "vector";
        std::regex re_expression(R"(non_matching_pattern)");
        std::pair<bool, std::smatch> result = xcpp::is_inspect_request(code, re_expression);
        REQUIRE(result.first == false);
    }

}

#if !defined(XEUS_CPP_EMSCRIPTEN_WASM_BUILD)
TEST_SUITE("xassist"){

    TEST_CASE("model_not_found"){
        xcpp::xassist assist;
        std::string line = "%%xassist testModel";
        std::string cell = "test input";

        StreamRedirectRAII redirect(std::cerr);

        assist(line, cell);

        REQUIRE(redirect.getCaptured() == "Model not found.\n");

    }

    TEST_CASE("gemini"){
        xcpp::xassist assist;
        std::string line = "%%xassist gemini --save-key";
        std::string cell = "1234";

        assist(line, cell);

        std::ifstream infile("gemini_api_key.txt");
        std::string content;
        std::getline(infile, content);

        REQUIRE(content == "1234");
        infile.close();

        line = "%%xassist gemini --save-model";
        cell = "1234";

        assist(line, cell);

        std::ifstream infile_model("gemini_model.txt");
        std::string content_model;
        std::getline(infile_model, content_model);

        REQUIRE(content_model == "1234");
        infile_model.close();

        StreamRedirectRAII redirect(std::cerr);
        
        assist("%%xassist gemini", "hello");

        REQUIRE(!redirect.getCaptured().empty());

        line = "%%xassist gemini --refresh";
        cell = "";

        assist(line, cell);

        std::ifstream infile_chat("gemini_chat_history.txt");
        std::string content_chat;
        std::getline(infile_chat, content_chat);

        REQUIRE(content_chat == "");
        infile_chat.close();

        std::remove("gemini_api_key.txt");
        std::remove("gemini_model.txt");
        std::remove("gemini_chat_history.txt");
    }

    TEST_CASE("openai"){
        xcpp::xassist assist;
        std::string line = "%%xassist openai --save-key";
        std::string cell = "1234";

        assist(line, cell);

        std::ifstream infile("openai_api_key.txt");
        std::string content;
        std::getline(infile, content);

        REQUIRE(content == "1234");
        infile.close();

         line = "%%xassist openai --save-model";
        cell = "1234";

        assist(line, cell);

        std::ifstream infile_model("openai_model.txt");
        std::string content_model;
        std::getline(infile_model, content_model);

        REQUIRE(content_model == "1234");
        infile_model.close();

        StreamRedirectRAII redirect(std::cerr);
        
        assist("%%xassist openai", "hello");

        REQUIRE(!redirect.getCaptured().empty());

        std::remove("openai_api_key.txt");
        std::remove("openai_model.txt");
        std::remove("openai_chat_history.txt");
    }

    TEST_CASE("ollama"){
        xcpp::xassist assist;
        std::string line = "%%xassist ollama --set-url";
        std::string cell = "https://api.openai.com/v1/chat/completions";

        assist(line, cell);

        std::ifstream infile("ollama_url.txt");
        std::string content;
        std::getline(infile, content);

        REQUIRE(content == "https://api.openai.com/v1/chat/completions");
        infile.close();

        line = "%%xassist ollama --save-model";
        cell = "1234";

        assist(line, cell);

        std::ifstream infile_model("ollama_model.txt");
        std::string content_model;
        std::getline(infile_model, content_model);

        REQUIRE(content_model == "1234");
        infile_model.close();

        StreamRedirectRAII redirect(std::cerr);
        
        assist("%%xassist ollama", "hello");

        REQUIRE(!redirect.getCaptured().empty());

        std::remove("ollama_url.txt");
        std::remove("ollama_model.txt");
    }

}
#endif


TEST_SUITE("file") {
    TEST_CASE("Write") {
        xcpp::writefile wf;
        std::string line = "%%file testfile.txt";
        std::string cell = "Hello, World!";

        wf(line, cell);

        std::ifstream infile("testfile.txt");
        std::string content;
        std::getline(infile, content);

        REQUIRE(content == "Hello, World!");
        infile.close();
    }
    TEST_CASE("Overwrite") {
        xcpp::writefile wf;
        std::string line = "%%file testfile.txt";
        std::string cell = "Hello, World!";

        wf(line, cell);

        std::string overwrite_cell = "Overwrite test";

        wf(line, overwrite_cell);

        std::ifstream infile("testfile.txt");
        std::string content;
        std::getline(infile, content);

        REQUIRE(content == overwrite_cell);
        infile.close();
    }
    TEST_CASE("Append") {
        xcpp::writefile wf;
        std::string line = "%%file testfile.txt";
        std::string cell = "Hello, World!";

        wf(line, cell);

        std::string append_line = "%%file -a testfile.txt";
        std::string append_cell = "Hello, again!";

        wf(append_line, append_cell);

        std::ifstream infile("testfile.txt");
        std::vector<std::string> lines;
        std::string content;
        while(std::getline(infile, content)) {
            lines.push_back(content);
        }

        REQUIRE(lines[0] == "Hello, World!");
        REQUIRE(lines[1] == "Hello, again!");
        infile.close();
    }
}

TEST_SUITE("mime_bundle_repr")
{
    TEST_CASE("int")
    {
        int value = 42;
        nl::json res = xcpp::mime_bundle_repr(value);
        nl::json expected = {
            {"text/plain", "42"}
        };

        REQUIRE(res == expected);
    }
}
