/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#ifndef XMAGICS_EXECUTION_HPP
#define XMAGICS_EXECUTION_HPP

#include <cstddef>
#include <string>

#include "xeus-cpp/xmagics.hpp"
#include "xeus-cpp/xoptions.hpp"

namespace xcpp
{
    class timeit : public xmagic_line_cell
    {
    public:

        XEUS_CPP_API
        virtual void operator()(const std::string& line) override;

        XEUS_CPP_API
        virtual void operator()(const std::string& line, const std::string& cell) override;

    public:

        static int exec_counter;

    private:

        void get_options(argparser& argpars);
        std::string inner(std::size_t number, const std::string& code, int exec_counter) const;
        std::string _format_time(double timespan, std::size_t precision) const;
        void execute(std::string& line, std::string& cell);
    };
}
#endif