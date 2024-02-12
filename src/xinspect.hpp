/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 * Copyright (c) 2023, Martin Vassilev                                              *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/
#ifndef XEUS_CPP_INSPECT_HPP
#define XEUS_CPP_INSPECT_HPP

#include <filesystem>
#include <fstream>
#include <string>

#include <pugixml.hpp>

#include <xtl/xsystem.hpp>

#include "xeus-cpp/xbuffer.hpp"
#include "xeus-cpp/xpreamble.hpp"

#include "xdemangle.hpp"
#include "xparser.hpp"

//#include "llvm/Support/FileSystem.h"
//#include "llvm/Support/Path.h"

//#include "clang/Interpreter/CppInterOp.h"


namespace xcpp
{
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


    std::string find_type_slow(const std::string& expression) {
        static unsigned long long var_count = 0;

        if (auto type = Cpp::GetType(expression))
            return Cpp::GetQualifiedName(type);

        // Here we might need to deal with integral types such as 3.14.

        std::string id = "__Xeus_GetType_" + std::to_string(var_count++);
        std::string using_clause = "using " + id + " = __typeof__(" + expression + ");\n";

        if (!Cpp::Declare(using_clause.c_str(), /*silent=*/false)) {
            Cpp::TCppScope_t lookup = Cpp::GetNamed(id, 0);
            Cpp::TCppType_t lookup_ty = Cpp::GetTypeFromScope(lookup);
            return Cpp::GetQualifiedCompleteName(Cpp::GetCanonicalType(lookup_ty));
        }
        return "";
    }
/*
    std::string find_type(const std::string& expression)
    {
        auto PTU = interpreter.Parse(expression + ";");
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
*/
    static nl::json read_tagconfs(const char* path)
    {
        nl::json result = nl::json::array();
        for (auto &entry: std::filesystem::directory_iterator(path)) {
            if (entry.path().extension() != ".json")
              continue;
            std::ifstream i(entry.path());
            nl::json json_entry;
            i >> json_entry;
            result.emplace_back(std::move(json_entry));
        }
        return result;
/*
        std::error_code EC;
        for (llvm::sys::fs::directory_iterator File(path, EC), FileEnd;
             File != FileEnd && !EC; File.increment(EC)) {
          llvm::StringRef FilePath = File->path();
          llvm::StringRef FileName = llvm::sys::path::filename(FilePath);
          if (!FileName.endswith("json"))
            continue;
          std::ifstream i(FilePath.str());
          nl::json entry;
          i >> entry;
          result.emplace_back(std::move(entry));
        }
        return result;
*/
    }

    std::pair<bool, std::smatch> is_inspect_request(const std::string code, std::regex re)
    {
        std::smatch inspect;
        if (std::regex_search(code, inspect, re)){
            return std::make_pair(true, inspect);
        }
        return std::make_pair(false, inspect);
    }

    void inspect(const std::string& code, nl::json& kernel_res)
    {
        std::string tagconf_dir = XCPP_TAGCONFS_DIR;
        std::string tagfiles_dir = XCPP_TAGFILES_DIR;

        nl::json tagconfs = read_tagconfs(tagconf_dir.c_str());

        std::vector<std::string> check{"class", "struct", "function"};

        std::string url, tagfile;

        std::regex re_expression(R"((((?:\w*(?:\:{2}|\<.*\>|\(.*\)|\[.*\])?)\.?)*))");

        std::smatch inspect = is_inspect_request(code, re_expression).second;

        std::string inspect_result;

        std::smatch method;
        std::string to_inspect = inspect[1];

        // Method or variable of class found (xxxx.yyyy)
        if (std::regex_search(to_inspect, method, std::regex(R"((.*)\.(\w*)$)")))
        {
            std::string typename_ = find_type_slow(method[1]);

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
                std::string typename_ = find_type_slow(to_inspect);
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

/*
    class xintrospection : public xpreamble
    {
    public:

        using xpreamble::pattern;
        const std::string spattern = R"(^\?)";

        xintrospection(clang::Interpreter& p)
            : m_interpreter{p}
        {
            pattern = spattern;
        }

        void apply(const std::string& code, nl::json& kernel_res) override
        {
            std::regex re(spattern + R"((.*))");
            std::smatch to_inspect;
            std::regex_search(code, to_inspect, re);
            inspect(to_inspect[1], kernel_res, m_interpreter);
        }

        virtual xpreamble* clone() const override
        {
            return new xintrospection(*this);
        }

    private:

        clang::Interpreter& m_interpreter;
    };
*/
}
#endif
