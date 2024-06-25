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
#include <sstream>
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

    std::vector<std::string> get_lines(const std::string& input)
    {
        std::vector<std::string> lines;
        std::regex re("\\n");

        std::copy(
            std::sregex_token_iterator(input.begin(), input.end(), re, -1),
            std::sregex_token_iterator(),
            std::back_inserter(lines)
        );
        return lines;
    }

    std::vector<std::string> split_from_includes(const std::string& input)
    {
        // this function split the input into part where we have only #include.

        // split input into lines
        std::vector<std::string> lines = get_lines(input);

        // check if each line contains #include and concatenate the result in the good part of the result
        std::regex incl_re("\\#include.*");
        std::regex magic_re("^\\%\\w+");
        std::vector<std::string> result;
        result.push_back("");
        std::size_t current = 0;  // 0 include, 1 other
        std::size_t rindex = 0;   // current index of result vector
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (!lines[i].empty())
            {
                if (std::regex_search(lines[i], magic_re))
                {
                    result.push_back(lines[i] + "\n");
                    result.push_back("");
                    rindex += 2;
                }
                else
                {
                    if (std::regex_match(lines[i], incl_re))
                    {
                        // if we have #include in this line
                        // but the current item of result vector contains
                        // other things
                        if (current != 0)
                        {
                            current = 0;
                            result.push_back("");
                            rindex++;
                        }
                    }
                    else
                    {
                        // if we don't have #include in this line
                        // but the current item of result vector contains
                        // the include parts
                        if (current != 1)
                        {
                            current = 1;
                            result.push_back("");
                            rindex++;
                        }
                    }
                    // if we have multiple lines, we add a semicolon at the end of the lines that not contain
                    // #include keyword (except for the last line)
                    result[rindex] += lines[i];
                    if (i != lines.size() - 1)
                    {
                        result[rindex] += "\n";
                    }
                }
            }
        }
        return result;
    }
}
