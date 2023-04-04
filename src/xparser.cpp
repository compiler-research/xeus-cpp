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
#include <string>

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
}
