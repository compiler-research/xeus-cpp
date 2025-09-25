/************************************************************************************
 * Copyright (c) 2025, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#ifndef XEUS_CPP_MULTI_INTERPRETER_MAGIC_HPP
#define XEUS_CPP_MULTI_INTERPRETER_MAGIC_HPP

#include <string>
#include <unordered_map>

#include "CppInterOp/CppInterOp.h"
#include "xeus-cpp/xmagics.hpp"

namespace xcpp
{
    class multi_interpreter : public xmagic_cell
    {
    public:

        XEUS_CPP_API
        void operator()(const std::string& line, const std::string& cell) override;

    private:

        std::unordered_map<std::string, Cpp::TInterp_t> interpreters;
    };
}  // namespace xcpp

#endif  // XEUS_CPP_MULTI_INTERPRETER_MAGIC_HPP
