/************************************************************************************
 * Copyright (c) 2025, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/
#ifndef XEUS_CPP_INTERNAL_UTILS_HPP
#define XEUS_CPP_INTERNAL_UTILS_HPP

#include <string>

#include "xeus/xsystem.hpp"

namespace xcpp
{
    // Temporary file utilities for Jupyter cells
    std::string get_tmp_prefix();
    std::string get_tmp_suffix();
    std::string get_cell_tmp_file(const std::string& content);
}

#endif
