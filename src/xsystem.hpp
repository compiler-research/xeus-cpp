/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 * Copyright (c) 2023, Johan Mabille, Loic Gouarin, Sylvain Corlay, Wolf Vollprecht *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#ifndef XEUS_CPP_SYSTEM_HPP
#define XEUS_CPP_SYSTEM_HPP

#include <cstdio>

#include "xeus-cpp/xpreamble.hpp"

namespace xcpp
{
    struct xsystem : xpreamble
    {
        const std::string spattern = R"(^\!)";
        using xpreamble::pattern;

        xsystem()
        {
            pattern = spattern;
        }

        void apply(const std::string& code, nl::json& kernel_res) override
        {
            std::regex re(spattern + R"((.*))");
            std::smatch to_execute;
            std::regex_search(code, to_execute, re);

            int ret = 1; // ???

            // Redirection of stderr to stdout
            std::string command = to_execute.str(1) + " 2>&1";

#if defined(WIN32)
            FILE* shell_result = _popen(command.c_str(), "r");
#else
            FILE* shell_result = popen(command.c_str(), "r");
#endif
            if (shell_result)
            {
                char buff[512];

                ret = 0; // ???

                while (fgets(buff, sizeof(buff), shell_result))
                {
                    std::cout << buff;
                }
#if defined(WIN32)
                _pclose(shell_result);
#else
                pclose(shell_result);
#endif

                std::cout << std::flush;
                kernel_res["status"] = "ok";
            }
            else
            {
                std::cerr << "Unable to execute the shell command\n";
                std::cout << std::flush;
                kernel_res["status"] = "error";
                kernel_res["ename"] = "ename";
                kernel_res["evalue"] = "evalue";
                kernel_res["traceback"] = nl::json::array();
            }
        }

        virtual xpreamble* clone() const override
        {
            return new xsystem(*this);
        }
    };
}
#endif
