/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 * Copyright (c) 2023, Johan Mabille, Loic Gouarin, Sylvain Corlay, Wolf Vollprecht *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#ifndef XEUS_CPP_PARSER_HPP
#define XEUS_CPP_PARSER_HPP

#include "xeus-cpp/xeus_cpp_config.hpp"

#include <string>
#include <vector>

namespace xcpp
{
    XEUS_CPP_API
    std::string trim(const std::string& str);

    XEUS_CPP_API std::vector<std::string>
    split_line(const std::string& input, const std::string& delims, std::size_t cursor_pos);

    XEUS_CPP_API
    std::vector<std::string> get_lines(const std::string& input);

    XEUS_CPP_API
    std::vector<std::string> split_from_includes(const std::string& input);
}
#endif
