/************************************************************************************
 * Copyright (c) 2025, xeus-cpp contributors                                        *
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

#include <nlohmann/json_fwd.hpp>

#include "xeus-cpp/xpreamble.hpp"
#include "xeus-cpp/xutils.hpp"

#include "xparser.hpp"

namespace xcpp
{
    struct XEUS_CPP_API node_predicate
    {
        std::string kind;
        std::string child_value;

        bool operator()(pugi::xml_node node) const;
    };

    struct XEUS_CPP_API class_member_predicate
    {
        std::string class_name;
        std::string kind;
        std::string child_value;

        std::string get_filename(pugi::xml_node node) const;
        bool operator()(pugi::xml_node node) const;
    };

    std::string inspect(const std::string& code);
    nl::json build_inspect_data(const std::string& inspect_result);

    class XEUS_CPP_API xintrospection : public xpreamble
    {
    public:

        using xpreamble::pattern;
        const std::string spattern = R"(^\?)";

        xintrospection();

        void apply(const std::string& code, nl::json& kernel_res) override;

        [[nodiscard]] std::unique_ptr<xpreamble> clone() const override;
    };
}

#endif // XEUS_CPP_INSPECT_HPP
