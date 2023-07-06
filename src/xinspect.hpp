/***********************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 * Copyright (c) 2023, Johan Mabille, Loic Gouarin, Sylvain Corlay, Wolf Vollprecht *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/
#ifndef XEUS_CPP_INSPECT_HPP
#define XEUS_CPP_INSPECT_HPP

#include <fstream>
#include <string>

#include <dirent.h>

#include <pugixml.hpp>

#include <xtl/xsystem.hpp>

#include "xeus-cpp/xbuffer.hpp"
#include "xeus-cpp/xpreamble.hpp"

#include "xdemangle.hpp"
#include "xparser.hpp"

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

    std::string find_type(const std::string& expression, clang::Interpreter& interpreter)
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

    std::pair<bool, std::smatch> is_inspect_request(const std::string code, std::regex re) 
    {
        std::smatch inspect;
        if (std::regex_search(code, inspect, re)){
            return std::make_pair(true, inspect);
        }
        return std::make_pair(false, inspect);
       
    }

    void inspect(const std::string& code, nl::json& kernel_res, clang::Interpreter& interpreter)
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
}
#endif
