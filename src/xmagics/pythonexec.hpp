/************************************************************************************
 * Copyright (c) 2025, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#ifndef XEUS_CPP_PYTHONEXEC_MAGIC_HPP
#define XEUS_CPP_PYTHONEXEC_MAGIC_HPP

#include <string>

#include "xeus-cpp/xmagics.hpp"

namespace xcpp
{
    class pythonexec : public xmagic_cell
    {
    public:
        XEUS_CPP_API
        void operator()(const std::string& line, const std::string& cell) override;
    private:
        static bool is_initalized;
        void initialize();

    };
}  // namespace xcpp

#endif //XEUS_CPP_PYTHONEXEC_MAGIC_HPP
