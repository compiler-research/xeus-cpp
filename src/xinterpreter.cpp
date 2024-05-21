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
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#if defined(__linux__)
#  include <unistd.h>
#endif
#if defined(_WIN32)
#  if defined(NOMINMAX)
#    include <windows.h>
#  else
#    define NOMINMAX
#    include <windows.h>
#    undef NOMINMAX
#  endif
#endif
#ifdef __APPLE__
#  include <cstdint>
#  include <mach-o/dyld.h>
#endif
#if defined(__sun)
#  include <stdlib.h>
#endif
#ifdef __FreeBSD__
#  include <sys/types.h>
#  include <sys/sysctl.h>
#endif

#include <xeus/xhelper.hpp>

#include "xeus-cpp/xbuffer.hpp"
#include "xeus-cpp/xeus_cpp_config.hpp"

#include "xeus-cpp/xinterpreter.hpp"
#include "xeus-cpp/xmagics.hpp"

#include "xinput.hpp"
#include "xinspect.hpp"
#include "xmagics/os.hpp"
#include "xparser.hpp"
#include "xsystem.hpp"

using Args = std::vector<const char*>;

void* createInterpreter(const Args &ExtraArgs = {}) {
  Args ClangArgs = {/*"-xc++"*/"-v"}; // ? {"-Xclang", "-emit-llvm-only", "-Xclang", "-diagnostic-log-file", "-Xclang", "-", "-xc++"};
  if (std::find(ExtraArgs.begin(), ExtraArgs.end(), "-resource-dir") != ExtraArgs.end()) {
    std::string resource_dir = Cpp::DetectResourceDir();
    if (resource_dir.empty())
      std::cerr << "Failed to detect the resource-dir\n";
    ClangArgs.push_back("-resource-dir");
    ClangArgs.push_back(resource_dir.c_str());
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
  return Cpp::CreateInterpreter(ClangArgs/*, {"-cuda"}*/);
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

    inline std::string executable_path()
    {
        std::string path;
#if defined(UNICODE)
    wchar_t buffer[1024];
#else
    char buffer[1024];
#endif
        std::memset(buffer, '\0', sizeof(buffer));
#if defined(__linux__)
        if (readlink("/proc/self/exe", buffer, sizeof(buffer)) != -1)
        {
            path = buffer;
        }
        else
        {
            // failed to determine run path
        }
#elif defined (_WIN32)
    #if defined(UNICODE)
        if (GetModuleFileNameW(nullptr, buffer, sizeof(buffer)) != 0)
        {
            // Convert wchar_t to std::string
            std::wstring wideString(buffer);
            std::string narrowString(wideString.begin(), wideString.end());
            path = narrowString;
        }
    #else
        if (GetModuleFileNameA(nullptr, buffer, sizeof(buffer)) != 0)
        {
            path = buffer;
        }
    #endif
        // failed to determine run path
#elif defined (__APPLE__)
        std::uint32_t size = sizeof(buffer);
        if(_NSGetExecutablePath(buffer, &size) == 0)
        {
            path = buffer;
        }
        else
        {
            // failed to determine run path
        }
#elif defined (__FreeBSD__)
        int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
        size_t buffer_size = sizeof(buffer);
        if (sysctl(mib, 4, buffer, &buffer_size, NULL, 0) != -1)
        {
            path = buffer;
        }
        else
        {
            // failed to determine run path
        }
#elif defined(__sun)
        path = getexecname();
#endif
        return path;
    }

    inline std::string prefix_path()
    {
        std::string path = executable_path();
#if defined (_WIN32)
        char separator = '\\';
#else
        char separator = '/';
#endif
        std::string bin_folder = path.substr(0, path.find_last_of(separator));
        std::string prefix = bin_folder.substr(0, bin_folder.find_last_of(separator)) + separator;
        return prefix;
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


        auto input_guard = input_redirection(allow_stdin);

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
        Cpp::AddIncludePath((prefix_path() + "/include/").c_str());
    }

    void interpreter::init_preamble()
    {
        // FIXME: Make register_preamble take a unique_ptr.
        //NOLINTBEGIN(cppcoreguidelines-owning-memory)
        preamble_manager.register_preamble("introspection", new xintrospection());
        preamble_manager.register_preamble("magics", new xmagics_manager());
        preamble_manager.register_preamble("shell", new xsystem());
        //NOLINTEND(cppcoreguidelines-owning-memory)
    }

    void interpreter::init_magic()
    {
        // preamble_manager["magics"].get_cast<xmagics_manager>().register_magic("executable", executable(m_interpreter));
        // preamble_manager["magics"].get_cast<xmagics_manager>().register_magic("file", writefile());
        // preamble_manager["magics"].get_cast<xmagics_manager>().register_magic("timeit", timeit(&m_interpreter));
        // preamble_manager["magics"].get_cast<xmagics_manager>().register_magic("python", pythonexec());
    }
}
