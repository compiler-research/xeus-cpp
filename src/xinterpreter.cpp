/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 * Copyright (c) 2023, Martin Vassilev                                              *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#include "xeus/xhelper.hpp"
#include "xeus/xsystem.hpp"

#include "xeus-cpp/xbuffer.hpp"
#include "xeus-cpp/xeus_cpp_config.hpp"
#include "xeus-cpp/xinterpreter.hpp"
#include "xeus-cpp/xmagics.hpp"

#include "xinput.hpp"
#include "xinspect.hpp"
#include "xmagics/os.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#ifndef EMSCRIPTEN
#include "xmagics/xassist.hpp"
#endif
#include "xparser.hpp"
#include "xsystem.hpp"

using Args = std::vector<const char*>;

void* createInterpreter(const Args &ExtraArgs = {}) {
  Args ClangArgs = {/*"-xc++"*/"-v"};
  if (std::find_if(ExtraArgs.begin(), ExtraArgs.end(), [](const std::string& s) {
    return s == "-resource-dir";}) == ExtraArgs.end()) {
    std::string resource_dir = Cpp::DetectResourceDir();
    if (!resource_dir.empty()) {
        ClangArgs.push_back("-resource-dir");
        ClangArgs.push_back(resource_dir.c_str());
    } else {
        std::cerr << "Failed to detect the resource-dir\n";
    }
  }
  std::vector<std::string> CxxSystemIncludes;
  Cpp::DetectSystemCompilerIncludePaths(CxxSystemIncludes);
  for (const std::string& CxxInclude : CxxSystemIncludes) {
    ClangArgs.push_back("-isystem");
    ClangArgs.push_back(CxxInclude.c_str());
  }
  ClangArgs.insert(ClangArgs.end(), ExtraArgs.begin(), ExtraArgs.end());
  // FIXME: We should process the kernel input options and conditionally pass
  // the gpu args here.
  Cpp::TInterp_t res = Cpp::CreateInterpreter(ClangArgs /*, {"-cuda"}*/);
  if (!res)
  {
      return res;
  }

  // clang-repl does not load libomp.so when -fopenmp flag is used
  // we need to explicitly load it
  if (std::find_if(
          ClangArgs.begin(),
          ClangArgs.end(),
          [](const std::string& s)
          {
              return s.find("-fopenmp") == 0;
          }
      )
      != ClangArgs.end())
  {
      if (!Cpp::LoadLibrary("libomp"))
      {
          std::cerr << "Failed to load libomp)" << std::endl;
      }
  }
  return res;
}

using namespace std::placeholders;

namespace xcpp
{
    struct StreamRedirectRAII {
      std::string &err;
      StreamRedirectRAII(std::string &e) : err(e) {
        Cpp::BeginStdStreamCapture(Cpp::kStdErr);
        Cpp::BeginStdStreamCapture(Cpp::kStdOut);
      }
      ~StreamRedirectRAII() {
        std::string out = Cpp::EndStdStreamCapture();
        err = Cpp::EndStdStreamCapture();
        std::cout << out;
      }
    };

    void interpreter::configure_impl()
    {
        xeus::register_interpreter(this);
    }

    static std::string get_stdopt()
    {
        // We need to find what's the C++ version the interpreter runs with.
        const char* code = R"(
int __get_cxx_version () {
#if __cplusplus > 202302L
    return 26;
#elif __cplusplus > 202002L
    return 23;
#elif __cplusplus >  201703L
    return 20;
#elif __cplusplus > 201402L
    return 17;
#elif __cplusplus > 201103L || (defined(_WIN32) && _MSC_VER >= 1900)
    return 14;
#elif __cplusplus >= 201103L
   return 11;
#else
  return 0;
#endif
  }
__get_cxx_version ()
      )";
        auto cxx_version = Cpp::Evaluate(code);
        return std::to_string(cxx_version);
    }

    interpreter::interpreter(int argc, const char* const* argv) :
        xmagics()
        , p_cout_strbuf(nullptr)
        , p_cerr_strbuf(nullptr)
        , m_cout_buffer(std::bind(&interpreter::publish_stdout, this, _1))
        , m_cerr_buffer(std::bind(&interpreter::publish_stderr, this, _1))
    {
        //NOLINTNEXTLINE (cppcoreguidelines-pro-bounds-pointer-arithmetic)
        createInterpreter(Args(argv ? argv + 1 : argv, argv + argc));
        m_version = get_stdopt();
        redirect_output();
        init_preamble();
        init_magic();
    }

    interpreter::~interpreter()
    {
        restore_output();
    }

    void interpreter::execute_request_impl(
        send_reply_callback cb,
        int /*execution_count*/,
        const std::string& code,
        xeus::execute_request_config config,
        nl::json /*user_expressions*/
    )
    {
        nl::json kernel_res;


        auto input_guard = input_redirection(config.allow_stdin);

        // Check for magics
        for (auto& pre : preamble_manager.preamble)
        {
            if (pre.second.is_match(code))
            {
                pre.second.apply(code, kernel_res);
                cb(kernel_res);
                return;
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

        if (config.silent)
        {
            auto null = xnull();
            std::cout.rdbuf(&null);
            std::cerr.rdbuf(&null);
        }

        std::string err;

        // Attempt normal evaluation
        try
        {
            StreamRedirectRAII R(err);
            compilation_result = Cpp::Process(code.c_str());
        }
        catch (std::exception& e)
        {
            errorlevel = 1;
            ename = "Standard Exception: ";
            evalue = e.what();
        }
        catch (...)
        {
            errorlevel = 1;
            ename = "Error: ";
        }

        if (compilation_result)
        {
            errorlevel = 1;
            ename = "Error: ";
            evalue = "Compilation error! " + err;
            std::cerr << err;
        }

        // Flush streams
        std::cout << std::flush;
        std::cerr << std::flush;

        // Reset non-silent output buffers
        if (config.silent)
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
            if (!config.silent)
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
                if (!config.silent && output.hasValue() && trim(code).back() != ';')
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
        cb(kernel_res);
    }

    nl::json interpreter::complete_request_impl(const std::string& code, int cursor_pos)
    {
        std::vector<std::string> results;

        // split the input to have only the word in the back of the cursor
        std::string delims = " \t\n`!@#$^&*()=+[{]}\\|;:\'\",<>?.";
        std::size_t _cursor_pos = cursor_pos;
        auto text = split_line(code, delims, _cursor_pos);
        std::string to_complete = text.back().c_str();

        Cpp::CodeComplete(results, code.c_str(), 1, _cursor_pos + 1);

        return xeus::create_complete_reply(results /*matches*/,
            cursor_pos - to_complete.length() /*cursor_start*/,
            cursor_pos /*cursor_end*/
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
            inspect(inspect_request.second[0], kernel_res);
        }
        return kernel_res;
    }

    nl::json interpreter::is_complete_request_impl(const std::string& code)
    {
        if (!code.empty() && code[code.size() - 1] == '\\') {
            auto found = code.rfind('\n');
            if (found == std::string::npos)
                found = -1;
            auto found1 = found++;
            while (isspace(code[++found1])) ;
            return xeus::create_is_complete_reply("incomplete", code.substr(found, found1-found));
        }

        return xeus::create_is_complete_reply("complete");
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
                             "  xeus-cpp: a C++ Jupyter kernel - based on Clang-repl\n";
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

    void interpreter::init_preamble()
    {
        //NOLINTBEGIN(cppcoreguidelines-owning-memory)
        preamble_manager.register_preamble("introspection", std::make_unique<xintrospection>());
        preamble_manager.register_preamble("magics", std::make_unique<xmagics_manager>());
        preamble_manager.register_preamble("shell", std::make_unique<xsystem>());
        //NOLINTEND(cppcoreguidelines-owning-memory)
    }

    void interpreter::init_magic()
    {
        // preamble_manager["magics"].get_cast<xmagics_manager>().register_magic("executable",
        // executable(m_interpreter));
        // preamble_manager["magics"].get_cast<xmagics_manager>().register_magic("timeit",
        // timeit(&m_interpreter));
        // preamble_manager["magics"].get_cast<xmagics_manager>().register_magic("python", pythonexec());
        preamble_manager["magics"].get_cast<xmagics_manager>().register_magic("file", writefile());
#ifndef EMSCRIPTEN
        preamble_manager["magics"].get_cast<xmagics_manager>().register_magic("xassist", xassist());
#endif
    }
}
