/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 * Copyright (c) 2023, Johan Mabille, Loic Gouarin, Sylvain Corlay, Wolf Vollprecht *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#include "xparser.hpp"

#include <cstddef>
#include <regex>
#include <string>
#include <vector>

namespace xcpp
{
    std::string trim(const std::string& str)
    {
        if (str.empty())
        {
            return str;
        }

        std::size_t firstScan = str.find_first_not_of(' ');
        std::size_t first = firstScan == std::string::npos ? str.length() : firstScan;
        std::size_t last = str.find_last_not_of(' ');
        return str.substr(first, last - first + 1);
    }

    std::vector<std::string>
    split_line(const std::string& input, const std::string& delims, std::size_t cursor_pos)
    {
        // passing -1 as the submatch index parameter performs splitting
        std::vector<std::string> result;
        std::stringstream ss;

        ss << "[";
        for (auto c : delims)
        {
            ss << "\\" << c;
        }
        ss << "]";

        std::regex re(ss.str());

        std::copy(
            std::sregex_token_iterator(input.begin(), input.begin() + cursor_pos + 1, re, -1),
            std::sregex_token_iterator(),
            std::back_inserter(result)
        );

        return result;
    }
}
