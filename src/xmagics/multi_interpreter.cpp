/************************************************************************************
 * Copyright (c) 2025, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#include "multi_interpreter.hpp"

#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "CppInterOp/CppInterOp.h"

static std::vector<std::string> split(const std::string& input)
{
    std::istringstream buffer(input);
    std::vector<std::string> ret(
        (std::istream_iterator<std::string>(buffer)),
        std::istream_iterator<std::string>()
    );
    return ret;
}

static constexpr size_t len(std::string_view n)
{
    return n.size();
}

namespace xcpp
{
    void multi_interpreter::operator()(const std::string& line, const std::string& cell)
    {
        auto Args0 = split(line.substr(len("subinterp")));

        Cpp::TInterp_t OldI = Cpp::GetInterpreter();  // we need to restore the old interpreter
        Cpp::TInterp_t I = nullptr;
        std::string name;
        if (Args0[0] == "--use")
        {
            I = interpreters[Args0[1]];
        }
        else
        {
            auto named = std::find(Args0.begin(), Args0.end(), "--name");
            if (named != Args0.end())
            {
                name = *(named + 1);
            }
        }

        std::vector<const char*> Args(I ? 0 : Args0.size());
        if (!I)
        {
            for (auto start = Args0.begin(), end = Args0.end(); start != end; start++)
            {
                if (*start == "--name")
                {
                    start++;
                    continue;
                }
                Args.push_back((*start).c_str());
            }
        }

        Cpp::BeginStdStreamCapture(Cpp::kStdErr);
        Cpp::BeginStdStreamCapture(Cpp::kStdOut);
        if (!I)
        {
            I = Cpp::CreateInterpreter(Args);  // TODO: error handling
        }
        if (I)
        {
            Cpp::Declare(cell.c_str(), false, I);
        }
        std::cout << Cpp::EndStdStreamCapture();
        std::cerr << Cpp::EndStdStreamCapture();

        if (!name.empty())
        {
            interpreters[name] = I;          // TODO: check if this is redefinition
            Cpp::ActivateInterpreter(OldI);  // restoring old interpreter
        }
        else
        {
            Cpp::DeleteInterpreter(I);
        }
    }
}  // namespace xcpp
