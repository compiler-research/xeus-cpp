/************************************************************************************
 * Copyright (c) 2025, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <utility>

#include "xinspect.hpp"

#include "clang/Interpreter/CppInterOp.h"

namespace xcpp
{
    bool node_predicate::operator()(pugi::xml_node node) const
    {
        return static_cast<std::string>(node.attribute("kind").value()) == kind
               && static_cast<std::string>(node.child("name").child_value()) == child_value;
    }

    std::string class_member_predicate::get_filename(pugi::xml_node node) const
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

    bool class_member_predicate::operator()(pugi::xml_node node) const
    {
        std::string node_kind = node.attribute("kind").value();
        std::string node_name = node.child("name").child_value();

        bool is_class_or_struct = (node_kind == "class" || node_kind == "struct");
        bool name_matches = (node_name == class_name);
        bool parent = is_class_or_struct && name_matches;

        if (parent)
        {
            for (pugi::xml_node child : node.children())
            {
                if (static_cast<std::string>(child.attribute("kind").value()) == kind
                    && static_cast<std::string>(child.child("name").child_value()) == child_value)
                {
                    return true;
                }
            }
        }
        return false;
    }

    std::string find_type_slow(const std::string& expression)
    {
        static unsigned long long var_count = 0;

        if (auto* type = Cpp::GetType(expression))
        {
            return Cpp::GetQualifiedName(type);
        }

        std::string id = "__Xeus_GetType_" + std::to_string(var_count++);
        std::string using_clause = "using " + id + " = __typeof__(" + expression + ");\n";

        if (!Cpp::Declare(using_clause.c_str(), false))
        {
            Cpp::TCppScope_t lookup = Cpp::GetNamed(id, nullptr);
            Cpp::TCppType_t lookup_ty = Cpp::GetTypeFromScope(lookup);
            return Cpp::GetQualifiedCompleteName(Cpp::GetCanonicalType(lookup_ty));
        }
        return "";
    }

    nl::json read_tagconfs(const char* path)
    {
        nl::json result = nl::json::array();
        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            if (entry.path().extension() != ".json")
            {
                continue;
            }
            std::ifstream i(entry.path());
            nl::json json_entry;
            i >> json_entry;
            result.emplace_back(std::move(json_entry));
        }
        return result;
    }

    std::pair<bool, std::smatch> is_inspect_request(const std::string& code, const std::regex& re)
    {
        std::smatch inspect;
        if (std::regex_search(code, inspect, re))
        {
            return std::make_pair(true, inspect);
        }
        return std::make_pair(false, inspect);
    }

    void inspect(const std::string& code, nl::json& kernel_res)
    {
        std::string tagconf_dir = retrieve_tagconf_dir();
        std::string tagfiles_dir = retrieve_tagfile_dir();

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
            std::string type_name = find_type_slow(method[1]);

            if (!type_name.empty())
            {
                for (nl::json::const_iterator it = tagconfs.cbegin(); it != tagconfs.cend(); ++it)
                {
                    url = it->at("url");
                    tagfile = it->at("tagfile");
                    std::string filename = tagfiles_dir + "/" + tagfile;
                    pugi::xml_document doc;
                    pugi::xml_parse_result result = doc.load_file(filename.c_str());
                    class_member_predicate predicate{type_name, "function", method[2]};
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
                std::string type_name = find_type_slow(to_inspect);
                find_string = (type_name.empty()) ? to_inspect : type_name;
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

    xintrospection::xintrospection()
    {
        pattern = spattern;
    }

    void xintrospection::apply(const std::string& code, nl::json& kernel_res)
    {
        std::regex re(spattern + R"((.*))");
        std::smatch to_inspect;
        std::regex_search(code, to_inspect, re);
        inspect(to_inspect[1], kernel_res);
    }

    std::unique_ptr<xpreamble> xintrospection::clone() const
    {
        return std::make_unique<xintrospection>(*this);
    }
}
