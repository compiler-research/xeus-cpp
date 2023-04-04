/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 * Copyright (c) 2023, Johan Mabille, Loic Gouarin, Sylvain Corlay, Wolf Vollprecht *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#ifndef XEUS_CPP_OPTIONS_HPP
#define XEUS_CPP_OPTIONS_HPP

#include <string>

#include <argparse/argparse.hpp>

#include "xeus_cpp_config.hpp"

namespace xcpp
{
    struct argparser : public argparse::ArgumentParser
    {
        using base_type = argparse::ArgumentParser;
        using base_type::ArgumentParser;

        void parse(const std::string& line);
    };
}
#endif
