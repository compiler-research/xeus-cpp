/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 * Copyright (c) 2023, Johan Mabille, Loic Gouarin, Sylvain Corlay, Wolf Vollprecht *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#include "xeus-cpp/xinterpreter.hpp"

#include "xinput.hpp"
// #include "xinspect.hpp"
// #include "xmagics/executable.hpp"
// #include "xmagics/execution.hpp"
#include "xmagics/os.hpp"
#include "xparser.hpp"
#include "xsystem.hpp"

#include <xeus/xhelper.hpp>

#include "xeus-cpp/xbuffer.hpp"
#include "xeus-cpp/xeus_cpp_config.hpp"

#include "xeus-cpp/xmagics.hpp"

#include <xtl/xsystem.hpp>

#include <pugixml.hpp>

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/TargetSelect.h>

#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Lex/HeaderSearchOptions.h>
#include <clang/Lex/Preprocessor.h>
//#include <clang/Interpreter/Value.h>



#include <algorithm>
#include <cinttypes>  // required before including llvm/ExecutionEngine/Orc/LLJIT.h because missing llvm/Object/SymbolicFile.h
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <dirent.h>

std::string DiagnosticOutput;
llvm::raw_string_ostream DiagnosticsOS(DiagnosticOutput);
auto DiagPrinter = std::make_unique<clang::TextDiagnosticPrinter>(DiagnosticsOS, new clang::DiagnosticOptions());

///\returns true on error.
static bool
process_code(clang::Interpreter& Interp, const std::string& code, llvm::raw_string_ostream& error_stream)
{
    auto PTU = Interp.Parse(code);
    if (!PTU)
    {
        auto Err = PTU.takeError();
        error_stream << DiagnosticsOS.str();
        // avoid printing the "Parsing failed error"
        // llvm::logAllUnhandledErrors(std::move(Err), error_stream, "error: ");
        return true;
    }
    if (PTU->TheModule)
    {
        llvm::Error ex = Interp.Execute(*PTU);
        error_stream << DiagnosticsOS.str();
        if (code.substr(0, 3) == "int")
        {
            for (clang::Decl* D : PTU->TUPart->decls())
            {
                if (clang::VarDecl* VD = llvm::dyn_cast<clang::VarDecl>(D))
                {
                    auto Name = VD->getNameAsString();
                    auto Addr = Interp.getSymbolAddress(clang::GlobalDecl(VD));
                    if (!Addr)
                    {
                        llvm::logAllUnhandledErrors(std::move(Addr.takeError()), error_stream, "error: ");
                        return true;
                    }
                }
            }
        }
        else if (code.substr(0, 16) == "std::vector<int>")
        {
            for (clang::Decl* D : PTU->TUPart->decls())
            {
                if (clang::VarDecl* VD = llvm::dyn_cast<clang::VarDecl>(D))
                {
                    auto Name = VD->getNameAsString();
                    auto Addr = Interp.getSymbolAddress(clang::GlobalDecl(VD));
                    if (!Addr)
                    {
                        llvm::logAllUnhandledErrors(std::move(Addr.takeError()), error_stream, "error: ");
                        return true;
                    }
                }
            }
        }

        llvm::logAllUnhandledErrors(std::move(ex), error_stream, "error: ");
        return false;
    }
    return false;
}

using Args = std::vector<const char*>;

static std::unique_ptr<clang::Interpreter>
create_interpreter(const Args& ExtraArgs = {}, clang::DiagnosticConsumer* Client = nullptr)
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    Args ClangArgs = {"-Xclang", "-emit-llvm-only", "-Xclang", "-diagnostic-log-file", "-Xclang", "-", "-xc++"};
    ClangArgs.insert(ClangArgs.end(), ExtraArgs.begin(), ExtraArgs.end());
    auto CI = cantFail(clang::IncrementalCompilerBuilder::create(ClangArgs));
    if (Client)
    {
        CI->getDiagnostics().setClient(Client, /*ShouldOwnClient=*/false);
    }
    return cantFail(clang::Interpreter::create(std::move(CI)));
}

static void
inject_symbol(llvm::StringRef LinkerMangledName, llvm::JITTargetAddress KnownAddr, clang::Interpreter& Interp)
{
    using namespace llvm;
    using namespace llvm::orc;

    auto Symbol = Interp.getSymbolAddress(LinkerMangledName);  //, /*IncludeFromHost=*/true);

    if (Error Err = Symbol.takeError())
    {
        logAllUnhandledErrors(std::move(Err), errs(), "[IncrementalJIT] define() failed1: ");
        return;
    }

    // Nothing to define, we are redefining the same function. FIXME: Diagnose.
    if (*Symbol && (JITTargetAddress) *Symbol == KnownAddr)
    {
        return;
    }

    // Let's inject it
    bool Inserted;
    SymbolMap::iterator It;
    static llvm::orc::SymbolMap m_InjectedSymbols;

    llvm::orc::LLJIT* Jit = const_cast<llvm::orc::LLJIT*>(Interp.getExecutionEngine());
    JITDylib& DyLib = Jit->getMainJITDylib();

    std::tie(It, Inserted) = m_InjectedSymbols.try_emplace(
        Jit->getExecutionSession().intern(LinkerMangledName),
        JITEvaluatedSymbol(KnownAddr, JITSymbolFlags::Exported)
    );
    assert(Inserted && "Why wasn't this found in the initial Jit lookup?");

    // We want to replace a symbol with a custom provided one.
    if (Symbol && KnownAddr)
    {
        // The symbol be in the DyLib or in-process.
        if (auto Err = DyLib.remove({It->first}))
        {
            logAllUnhandledErrors(std::move(Err), errs(), "[IncrementalJIT] define() failed2: ");
            return;
        }
    }

    if (Error Err = DyLib.define(absoluteSymbols({*It})))
    {
        logAllUnhandledErrors(std::move(Err), errs(), "[IncrementalJIT] define() failed3: ");
    }
}

namespace utils
{
    void AddIncludePath(llvm::StringRef Path, clang::HeaderSearchOptions& HOpts)
    {
        bool Exists = false;
        for (const clang::HeaderSearchOptions::Entry& E : HOpts.UserEntries)
        {
            if ((Exists = E.Path == Path))
            {
                break;
            }
        }
        if (Exists)
        {
            return;
        }

        HOpts.AddPath(Path, clang::frontend::Angled, false /* IsFramework */, true /* IsSysRootRelative */);

        if (HOpts.Verbose)
        {
            //    std::clog << "Added include paths " << Path << std::endl;
        }
    }
}

void AddIncludePath(clang::Interpreter& Interp, llvm::StringRef Path)
{
    clang::CompilerInstance* CI = const_cast<clang::CompilerInstance*>(Interp.getCompilerInstance());
    clang::HeaderSearchOptions& HOpts = CI->getHeaderSearchOpts();

    // Save the current number of entries
    std::size_t Idx = HOpts.UserEntries.size();
    utils::AddIncludePath(Path, HOpts);

    clang::Preprocessor& PP = CI->getPreprocessor();
    clang::SourceManager& SM = CI->getSourceManager();
    clang::FileManager& FM = SM.getFileManager();
    clang::HeaderSearch& HSearch = PP.getHeaderSearchInfo();
    const bool isFramework = false;

    // Add all the new entries into Preprocessor
    for (; Idx < HOpts.UserEntries.size(); ++Idx)
    {
        const clang::HeaderSearchOptions::Entry& E = HOpts.UserEntries[Idx];
        if (auto DE = FM.getOptionalDirectoryRef(E.Path))
        {
            HSearch.AddSearchPath(
                clang::DirectoryLookup(*DE, clang::SrcMgr::C_User, isFramework),
                E.Group == clang::frontend::Angled
            );
        }
    }
}

using namespace std::placeholders;

namespace xcpp
{
    void interpreter::configure_impl()
    {
        // todo: why is error_stream necessary
        std::string error_message;
        llvm::raw_string_ostream error_stream(error_message);
        // Expose xinterpreter instance to interpreted C++
        process_code(*m_interpreter, "#include \"xeus/xinterpreter.hpp\"", error_stream);
        std::string code = "xeus::register_interpreter(static_cast<xeus::xinterpreter*>((void*)"
                           + std::to_string(intptr_t(this)) + "));";
        process_code(*m_interpreter, code.c_str(), error_stream);
    }

    interpreter::interpreter(int argc, const char* const* argv)
        : m_interpreter(std::move(create_interpreter(Args() /*argv + 1, argv + argc)*/, DiagPrinter.get())))
        , m_version(get_stdopt(argc, argv))
        ,  // Extract C++ language standard version from command-line option
        xmagics()
        , p_cout_strbuf(nullptr)
        , p_cerr_strbuf(nullptr)
        , m_cout_buffer(std::bind(&interpreter::publish_stdout, this, _1))
        , m_cerr_buffer(std::bind(&interpreter::publish_stderr, this, _1))
    {
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
        int execution_counter,
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

        std::string error_message;
        llvm::raw_string_ostream error_stream(error_message);
        // Attempt normal evaluation
        try
        {
            compilation_result = process_code(*m_interpreter, code, error_stream);
        }
        catch (std::exception& e)
        {
            errorlevel = 1;
            ename = "Standard Exception";
            evalue = e.what();
        }
        catch (...)
        {
            errorlevel = 1;
            ename = "Error";
        }

        if (compilation_result)
        {
            errorlevel = 1;
            ename = "Error";
            evalue = error_stream.str();
        }

        error_stream.str().clear();
        DiagnosticsOS.str().clear();

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
            std::vector<std::string> traceback({ename + ": " + evalue});
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
        return xeus::create_complete_reply(
            nl::json::array(), /*matches*/
            cursor_pos,        /*cursor_start*/
            cursor_pos         /*cursor_end*/
        );
    }

    struct node_predicate
    {
        std::string kind;
        std::string child_value;

        bool operator()(pugi::xml_node node) const
        {
            return static_cast<std::string>(node.attribute("kind").value()) == kind
                   && static_cast<std::string>(node.child("name").child_value()) == child_value;
        }
    };

    struct class_member_predicate
    {
        std::string class_name;
        std::string kind;
        std::string child_value;

        std::string get_filename(pugi::xml_node node)
        {
            for (pugi::xml_node child : node.children())
            {
                if (static_cast<std::string>(child.attribute("kind").value()) == kind
                    && static_cast<std::string>(child.child("name").child_value()) == child_value)
                {
                    return child.child("anchorfile").child_value();
                }
            }
            return "";
        }

        bool operator()(pugi::xml_node node)
        {
            auto parent = (static_cast<std::string>(node.attribute("kind").value()) == "class"
                           || static_cast<std::string>(node.attribute("kind").value()) == "struct")
                          && static_cast<std::string>(node.child("name").child_value()) == class_name;
            auto found = false;
            if (parent)
            {
                for (pugi::xml_node child : node.children())
                {
                    if (static_cast<std::string>(child.attribute("kind").value()) == kind
                        && static_cast<std::string>(child.child("name").child_value()) == child_value)
                    {
                        found = true;
                        break;
                    }
                }
            }
            return found;
        }
    };

    std::string find_type(const std::string& expression, clang::Interpreter& interpreter)
    {
      printf("K\n");
	auto PTU = interpreter.Parse(expression + ";");
	printf("%s\n", expression.c_str());
	if (llvm::Error Err = PTU.takeError()) {
	  llvm::logAllUnhandledErrors(std::move(Err), llvm::errs(), "error: ");
	  return "";
	}

	clang::Decl *D = *PTU->TUPart->decls_begin();
	if (!llvm::isa<clang::TopLevelStmtDecl>(D))
	  return "";

	clang::Expr *E = llvm::cast<clang::Expr>(llvm::cast<clang::TopLevelStmtDecl>(D)->getStmt());

	clang::QualType QT = E->getType();
        return  QT.getAsString();
    }

    static nl::json read_tagconfs(const char* path)
    {
        nl::json result = nl::json::array();
        DIR* directory = opendir(path);
        if (directory == nullptr)
        {
            return result;
        }
        dirent* item = readdir(directory);
        while (item != nullptr)
        {
            std::string extension = "json";
            if (item->d_type == DT_REG)
            {
                std::string fname = item->d_name;

                if (fname.find(extension, (fname.length() - extension.length())) != std::string::npos)
                {
                    std::ifstream i(path + ('/' + fname));
                    nl::json entry;
                    i >> entry;
                    result.emplace_back(std::move(entry));
                }
            }
            item = readdir(directory);
        }
        closedir(directory);
        return result;
    }

  void inspect(const std::string& code, nl::json& kernel_res, clang::Interpreter& interpreter)
    {
        std::string tagconf_dir = XCPP_TAGCONFS_DIR;
        std::string tagfiles_dir = XCPP_TAGFILES_DIR;

        nl::json tagconfs = read_tagconfs(tagconf_dir.c_str());

        std::vector<std::string> check{"class", "struct", "function"};

        std::string url, tagfile;

        std::regex re_expression(R"((((?:\w*(?:\:{2}|\<.*\>|\(.*\)|\[.*\])?)\.?)*))");
        std::smatch inspect;
        std::regex_search(code, inspect, re_expression);

        std::string inspect_result;

        std::smatch method;
        std::string to_inspect = inspect[1];

        // Method or variable of class found (xxxx.yyyy)
        if (std::regex_search(to_inspect, method, std::regex(R"((.*)\.(\w*)$)")))
        {
            std::string typename_ = find_type(method[1], interpreter);

            if (!typename_.empty())
            {
                for (nl::json::const_iterator it = tagconfs.cbegin(); it != tagconfs.cend(); ++it)
                {
                    url = it->at("url");
                    tagfile = it->at("tagfile");
                    std::string filename = tagfiles_dir + "/" + tagfile;
                    pugi::xml_document doc;
                    pugi::xml_parse_result result = doc.load_file(filename.c_str());
                    class_member_predicate predicate{typename_, "function", method[2]};
                    auto node = doc.find_node(predicate);
                    if (!node.empty())
                    {
                        inspect_result = url + predicate.get_filename(node);
                    }
                }
            }
        }
        else
        {
            std::string find_string;

            // check if we try to find the documentation of a namespace
            // if yes, don't try to find the type using typeid
            std::regex is_namespace(R"(\w+(\:{2}\w+)+)");
            std::smatch namespace_match;
            if (std::regex_match(to_inspect, namespace_match, is_namespace))
            {
                find_string = to_inspect;
            }
            else
            {
                std::string typename_ = find_type(to_inspect, interpreter);
                find_string = (typename_.empty()) ? to_inspect : typename_;
            }

            for (nl::json::const_iterator it = tagconfs.cbegin(); it != tagconfs.cend(); ++it)
            {
                url = it->at("url");
                tagfile = it->at("tagfile");
                std::string filename = tagfiles_dir + "/" + tagfile;
                pugi::xml_document doc;
                pugi::xml_parse_result result = doc.load_file(filename.c_str());
                for (auto c : check)
                {
                    node_predicate predicate{c, find_string};
                    std::string node;

                    if (c == "class" || c == "struct")
                    {
                        node = doc.find_node(predicate).child("filename").child_value();
                    }
                    else
                    {
                        node = doc.find_node(predicate).child("anchorfile").child_value();
                    }

                    if (!node.empty())
                    {
                        inspect_result = url + node;
                    }
                }
            }
        }

        if (inspect_result.empty())
        {
            std::cerr << "No documentation found for " << code << "\n";
            std::cout << std::flush;
            kernel_res["found"] = false;
            kernel_res["status"] = "error";
            kernel_res["ename"] = "No documentation found";
            kernel_res["evalue"] = "";
            kernel_res["traceback"] = nl::json::array();
        }
        else
        {
            // Format html content.
            std::string html_content = R"(<style>
            #pager-container {
                padding: 0;
                margin: 0;
                width: 100%;
                height: 100%;
            }
            .xcpp-iframe-pager {
                padding: 0;
                margin: 0;
                width: 100%;
                height: 100%;
                border: none;
            }
            </style>
            <iframe class="xcpp-iframe-pager" src=")"
                                       + inspect_result + R"(?action=purge"></iframe>)";

            // Note: Adding "?action=purge" suffix to force cppreference's
            // Mediawiki to purge the HTTP cache.

            kernel_res["payload"] = nl::json::array();
            kernel_res["payload"][0] = nl::json::object(
                {{"data", {{"text/plain", inspect_result}, {"text/html", html_content}}},
                 {"source", "page"},
                 {"start", 0}}
            );
            kernel_res["user_expressions"] = nl::json::object();

            std::cout << std::flush;
            kernel_res["found"] = true;
            kernel_res["status"] = "ok";
        }
    }

    nl::json interpreter::inspect_request_impl(const std::string& code, int cursor_pos, int /*detail_level*/)
    {
        nl::json kernel_res;

        auto dummy = code.substr(0, cursor_pos);
        // TODO: same pattern as in inspect function (keep only one)
        std::string exp = R"(\w*(?:\:{2}|\<.*\>|\(.*\)|\[.*\])?)";
        std::regex re_method{"(" + exp + R"(\.?)*$)"};
        std::smatch magic;
        if (std::regex_search(dummy, magic, re_method))
        {
            inspect(magic[0], kernel_res, *m_interpreter);
        }
        return kernel_res;
    }

    nl::json interpreter::is_complete_request_impl(const std::string& code)
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

    static int printf_jit(const char* format, ...)
    {
        std::va_list args;
        va_start(args, format);

        std::string buf = c_format(format, args);
        std::cout << buf;

        va_end(args);

        return buf.size();
    }

    static int fprintf_jit(std::FILE* stream, const char* format, ...)
    {
        std::va_list args;
        va_start(args, format);

        int ret;
        if (stream == stdout || stream == stderr)
        {
            std::string buf = c_format(format, args);
            if (stream == stdout)
            {
                std::cout << buf;
            }
            else if (stream == stderr)
            {
                std::cerr << buf;
            }
            ret = buf.size();
        }
        else
        {
            // Just forward to vfprintf.
            ret = vfprintf(stream, format, args);
        }

        va_end(args);

        return ret;
    }

    void interpreter::redirect_output()
    {
        p_cout_strbuf = std::cout.rdbuf();
        p_cerr_strbuf = std::cerr.rdbuf();

        std::cout.rdbuf(&m_cout_buffer);
        std::cerr.rdbuf(&m_cerr_buffer);

        // Inject versions of printf and fprintf that output to std::cout
        // and std::cerr (see implementation above).
        inject_symbol("printf", llvm::pointerToJITTargetAddress(printf_jit), *m_interpreter);
        inject_symbol("fprintf", llvm::pointerToJITTargetAddress(fprintf_jit), *m_interpreter);
    }

    void interpreter::restore_output()
    {
        std::cout.rdbuf(p_cout_strbuf);
        std::cerr.rdbuf(p_cerr_strbuf);

        // No need to remove the injected versions of [f]printf: As they forward
        // to std::cout and std::cerr, these are handled implicitly.
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
        AddIncludePath(*m_interpreter, xtl::prefix_path() + "/include/");
    }

    void interpreter::init_preamble()
    {
        //        preamble_manager.register_preamble("introspection", new xintrospection(m_interpreter));
        preamble_manager.register_preamble("magics", new xmagics_manager());
        preamble_manager.register_preamble("shell", new xsystem());
    }

    void interpreter::init_magic()
    {
        //        preamble_manager["magics"].get_cast<xmagics_manager>().register_magic("executable",
        //        executable(m_interpreter));
        //        preamble_manager["magics"].get_cast<xmagics_manager>().register_magic("file", writefile());
        //        preamble_manager["magics"].get_cast<xmagics_manager>().register_magic("timeit",
        //        timeit(&m_interpreter));
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
