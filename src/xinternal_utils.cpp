/************************************************************************************
 * Copyright (c) 2025, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/
#include "xinternal_utils.hpp"

#ifdef _WIN32
#include <Windows.h>
#else
#include <cstdlib>
#endif

namespace xcpp
{
    std::string get_tmp_prefix()
    {
        return xeus::get_tmp_prefix("xcpp");
    }

    std::string get_tmp_suffix()
    {
        return ".cpp";
    }

    std::string get_cell_tmp_file(const std::string& content)
    {
        return xeus::get_cell_tmp_file(get_tmp_prefix(),
                                       content,
                                       get_tmp_suffix());
    }
}
