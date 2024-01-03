/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 * Copyright (c) 2023, Johan Mabille, Loic Gouarin, Sylvain Corlay, Wolf Vollprecht *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#ifndef XEUS_CPP_MAGICS_HPP
#define XEUS_CPP_MAGICS_HPP

#include <map>
#include <memory>

#include "xoptions.hpp"
#include "xpreamble.hpp"

namespace xcpp
{
    enum struct xmagic_type
    {
        cell,
        line
    };

    struct xmagic_line
    {
        virtual ~xmagic_line() = default;
        virtual void operator()(const std::string& line) = 0;
    };

    struct xmagic_cell
    {
        virtual ~xmagic_cell() = default;
        virtual void operator()(const std::string& line, const std::string& cell) = 0;
    };

    struct xmagic_line_cell : public xmagic_line,
                              xmagic_cell
    {
    };
}
#endif
