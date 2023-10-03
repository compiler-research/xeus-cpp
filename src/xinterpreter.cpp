/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 * Copyright (c) 2023, Martin Vassilev                                              *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#include <algorithm>
#include <cinttypes>  // required before including llvm/ExecutionEngine/Orc/LLJIT.h because missing llvm/Object/SymbolicFile.h
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <xtl/xsystem.hpp>

#include <xeus/xhelper.hpp>

#include "xeus-cpp/xbuffer.hpp"
#include "xeus-cpp/xeus_cpp_config.hpp"

#include "xeus-cpp/xinterpreter.hpp"
#include "xeus-cpp/xmagics.hpp"

#include "xinput.hpp"
// #include "xinspect.hpp"
// #include "xmagics/executable.hpp"
// #include "xmagics/execution.hpp"
#include "xmagics/os.hpp"
#include "xmagics/pythonexec.hpp"
#include "xparser.hpp"
#include "xsystem.hpp"

using Args = std::vector<const char*>;

void* createInterpreter(const Args &ExtraArgs = {}) {
  Args ClangArgs = {/*"-xc++"*/}; // ? {"-Xclang", "-emit-llvm-only", "-Xclang", "-diagnostic-log-file", "-Xclang", "-", "-xc++"};
  ClangArgs.insert(ClangArgs.end(), ExtraArgs.begin(), ExtraArgs.end());
  // FIXME: We should process the kernel input options and conditionally pass
  // the gpu args here.
  return Cpp::CreateInterpreter(ClangArgs, {"-cuda"});
}

using namespace std::placeholders;

namespace xcpp
{
    void interpreter::configure_impl()
    {
    }

    interpreter::interpreter(int argc, const char* const* argv)
        : m_version(get_stdopt(argc, argv))
        ,  // Extract C++ language standard version from command-line option
        xmagics()
        , p_cout_strbuf(nullptr)
        , p_cerr_strbuf(nullptr)
        , m_cout_buffer(std::bind(&interpreter::publish_stdout, this, _1))
        , m_cerr_buffer(std::bind(&interpreter::publish_stderr, this, _1))
    {
        createInterpreter(Args(argv, argv + argc));
        redirect_output();
        // Bootstrap the execution engine
        init_includes();
        init_preamble();
        init_magic();
    }

    interpreter::~interpreter()
    {
        restore_output();
    }

    nl::json interpreter::execute_request_impl(
        int /*execution_counter*/,
        const std::string& code,
        bool silent,
        bool /*store_history*/,
        nl::json /*user_expressions*/,
        bool allow_stdin
    )
    {
        nl::json kernel_res;

        // Check for magics
        for (auto& pre : preamble_manager.preamble)
        {
            if (pre.second.is_match(code))
            {
                pre.second.apply(code, kernel_res);
                return kernel_res;
            }
        }

        auto errorlevel = 0;
        std::string ename;
        std::string evalue;
        bool compilation_result = false;

        // If silent is set to true, temporarily dismiss all std::cerr and
        // std::cout outputs resulting from `process_code`.

        auto cout_strbuf = std::cout.rdbuf();
        auto cerr_strbuf = std::cerr.rdbuf();

        if (silent)
        {
            auto null = xnull();
            std::cout.rdbuf(&null);
            std::cerr.rdbuf(&null);
        }

        // Scope guard performing the temporary redirection of input requests.
        auto input_guard = input_redirection(allow_stdin);

        std::string err;
        std::string out;

        // Attempt normal evaluation
        try
        {
	    std::string exp = R"(\w*(?:\:{2}|\<.*\>|\(.*\)|\[.*\])?)";
	    std::regex re(R"((\w*(?:\:{2}|\<.*\>|\(.*\)|\[.*\])?)(\.?)*$)");
            auto inspect_request = is_inspect_request(code, re);
            if (inspect_request.first)
                inspect(inspect_request.second[0], kernel_res, *m_interpreter);
            
            Cpp::BeginStdStreamCapture(Cpp::kStdErr);
            Cpp::BeginStdStreamCapture(Cpp::kStdOut);
            compilation_result = Cpp::Process(code.c_str());
            out = Cpp::EndStdStreamCapture();
            err = Cpp::EndStdStreamCapture();
            std::cout << out;
        }
        catch (std::exception& e)
        {
            errorlevel = 1;
            ename = "Standard Exception :";
            evalue = e.what();
        }
        catch (...)
        {
            errorlevel = 1;
            ename = "Error :";
        }
    
        if (compilation_result)
        {
            errorlevel = 1;
            ename = "Error";
            std::cerr << err;
        }

        // Flush streams
        std::cout << std::flush;
        std::cerr << std::flush;

        // Reset non-silent output buffers
        if (silent)
        {
            std::cout.rdbuf(cout_strbuf);
            std::cerr.rdbuf(cerr_strbuf);
        }

        // Depending of error level, publish execution result or execution
        // error, and compose execute_reply message.
        if (errorlevel)
        {
            // Classic Notebook does not make use of the "evalue" or "ename"
            // fields, and only displays the traceback.
            //
            // JupyterLab displays the "{ename}: {evalue}" if the traceback is
            // empty.
            if (evalue.size() < 4) {
                ename = " ";
            }
            std::vector<std::string> traceback({ename  + evalue});
            if (!silent)
            {
                publish_execution_error(ename, evalue, traceback);
            }

            // Compose execute_reply message.
            kernel_res["status"] = "error";
            kernel_res["ename"] = ename;
            kernel_res["evalue"] = evalue;
            kernel_res["traceback"] = traceback;
        }
        else
        {
            /*
                // Publish a mime bundle for the last return value if
                // the semicolon was omitted.
                if (!silent && output.hasValue() && trim(code).back() != ';')
                {
                    nl::json pub_data = mime_repr(output);
                    publish_execution_result(execution_counter, std::move(pub_data), nl::json::object());
                }
                */
            // Compose execute_reply message.
            kernel_res["status"] = "ok";
            kernel_res["payload"] = nl::json::array();
            kernel_res["user_expressions"] = nl::json::object();
        }
        return kernel_res;
    }

    nl::json interpreter::complete_request_impl(const std::string& /*code*/, int cursor_pos)
    {
        return xeus::create_complete_reply(
            nl::json::array(), /*matches*/
            cursor_pos,        /*cursor_start*/
            cursor_pos         /*cursor_end*/
        );
    }

    nl::json interpreter::inspect_request_impl(const std::string& code, int cursor_pos, int /*detail_level*/)
    {
        nl::json kernel_res;
        std::string exp = R"(\w*(?:\:{2}|\<.*\>|\(.*\)|\[.*\])?)";
        std::regex re(R"((\w*(?:\:{2}|\<.*\>|\(.*\)|\[.*\])?)(\.?)*$)");
        auto inspect_request = is_inspect_request(code.substr(0, cursor_pos), re);
        if (inspect_request.first)
        {
            inspect(inspect_request.second[0], kernel_res, *m_interpreter);
        }
        return kernel_res;
    }

    nl::json interpreter::is_complete_request_impl(const std::string& /*code*/)
    {
        return xeus::create_is_complete_reply("complete", "   ");
    }

    nl::json interpreter::kernel_info_request_impl()
    {
        nl::json result;
        result["implementation"] = "xeus-cpp";
        result["implementation_version"] = XEUS_CPP_VERSION;

        /* The jupyter-console banner for xeus-cpp is the following:
          __  _____ _   _ ___
          \ \/ / _ \ | | / __|
           >  <  __/ |_| \__ \
          /_/\_\___|\__,_|___/

          xeus-cpp: a C++ Jupyter kernel based on Clang
        */

        std::string banner = ""
                             "  __  _____ _   _ ___\n"
                             "  \\ \\/ / _ \\ | | / __|\n"
                             "   >  <  __/ |_| \\__ \\\n"
                             "  /_/\\_\\___|\\__,_|___/\n"
                             "\n"
                             "  xeus-cpp: a C++ Jupyter kernel - based on Clang\n";
        banner.append(m_version);
        result["banner"] = banner;
        result["language_info"]["name"] = "C++";
        result["language_info"]["version"] = m_version;
        result["language_info"]["mimetype"] = "text/x-c++src";
        result["language_info"]["codemirror_mode"] = "text/x-c++src";
        result["language_info"]["file_extension"] = ".cpp";
        result["help_links"] = nl::json::array();
        result["help_links"][0] = nl::json::object(
            {{"text", "Xeus-cpp Reference"}, {"url", "https://xeus-cpp.readthedocs.io"}}
        );
        result["status"] = "ok";
        return result;
    }

    void interpreter::shutdown_request_impl()
    {
        restore_output();
    }

    static std::string c_format(const char* format, std::va_list args)
    {
        // Call vsnprintf once to determine the required buffer length. The
        // return value is the number of characters _excluding_ the null byte.
        std::va_list args_bufsz;
        va_copy(args_bufsz, args);
        std::size_t bufsz = vsnprintf(NULL, 0, format, args_bufsz);
        va_end(args_bufsz);

        // Create an empty string of that size.
        std::string s(bufsz, 0);

        // Now format the data into this string and return it.
        std::va_list args_format;
        va_copy(args_format, args);
        // The second parameter is the maximum number of bytes that vsnprintf
        // will write _including_ the  terminating null byte.
        vsnprintf(&s[0], s.size() + 1, format, args_format);
        va_end(args_format);

        return s;
    }

    void interpreter::redirect_output()
    {
        p_cout_strbuf = std::cout.rdbuf();
        p_cerr_strbuf = std::cerr.rdbuf();

        std::cout.rdbuf(&m_cout_buffer);
        std::cerr.rdbuf(&m_cerr_buffer);
    }

    void interpreter::restore_output()
    {
        std::cout.rdbuf(p_cout_strbuf);
        std::cerr.rdbuf(p_cerr_strbuf);
    }

    void interpreter::publish_stdout(const std::string& s)
    {
        publish_stream("stdout", s);
    }

    void interpreter::publish_stderr(const std::string& s)
    {
        publish_stream("stderr", s);
    }

    void interpreter::init_includes()
    {
    }

    void interpreter::init_preamble()
    {
        preamble_manager.register_preamble("introspection", new xintrospection(*m_interpreter));
        preamble_manager.register_preamble("magics", new xmagics_manager());
        preamble_manager.register_preamble("shell", new xsystem());
    }

    void interpreter::init_magic()
    {
        // preamble_manager["magics"].get_cast<xmagics_manager>().register_magic("executable", executable(m_interpreter));
        // preamble_manager["magics"].get_cast<xmagics_manager>().register_magic("file", writefile());
        // preamble_manager["magics"].get_cast<xmagics_manager>().register_magic("timeit", timeit(&m_interpreter));
        preamble_manager["magics"].get_cast<xmagics_manager>().register_magic("python", pythonexec());
    }

    std::string interpreter::get_stdopt(int argc, const char* const* argv)
    {
        std::string res = "14";
        for (int i = 0; i < argc; ++i)
        {
            std::string tmp(argv[i]);
            auto pos = tmp.find("-std=c++");
            if (pos != std::string::npos)
            {
                res = tmp.substr(pos + 8);
                break;
            }
        }
        return res;
    }
}
