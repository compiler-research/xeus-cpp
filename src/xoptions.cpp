/***********************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 * Copyright (c) 2023, Johan Mabille, Loic Gouarin, Sylvain Corlay, Wolf Vollprecht *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "xeus-cpp/xoptions.hpp"

namespace xcpp
{
    void argparser::parse(const std::string& line)
    {
        std::istringstream iss(line);
        std::vector<std::string> opt_strings(
            (std::istream_iterator<std::string>(iss)),
            std::istream_iterator<std::string>()
        );

        std::vector<const char*> copt_strings;

        for (std::size_t i = 0; i < opt_strings.size(); ++i)
        {
            copt_strings.push_back(opt_strings[i].c_str());
        }

        int argc = copt_strings.size();
        auto argv = &copt_strings[0];

        try
        {
            base_type::parse_args(argc, argv);
        }
        catch (const std::runtime_error& err)
        {
            std::cerr << err.what() << std::endl;
        }
    }
}
