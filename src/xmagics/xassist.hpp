/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#ifndef XEUS_CPP_XASSIST_MAGIC_HPP
#define XEUS_CPP_XASSIST_MAGIC_HPP

#include <string>

#include "xeus-cpp/xmagics.hpp"

namespace xcpp
{
    class xassist : public xmagic_cell
    {
    public:

        XEUS_CPP_API
        void operator()(const std::string& line, const std::string& cell) override;
    };
}  // namespace xcpp
#endif