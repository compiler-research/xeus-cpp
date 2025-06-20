#ifndef XEUS_CPP_INTERNAL_UTILS_HPP
#define XEUS_CPP_INTERNAL_UTILS_HPP

#include <string>

#include "xeus/xsystem.hpp"

namespace xcpp
{
    // Colorize text for terminal output
    std::string red_text(const std::string& text);
    std::string green_text(const std::string& text);
    std::string blue_text(const std::string& text);

    // Placeholder for C++ syntax highlighting
    std::string highlight(const std::string& code);

    // Temporary file utilities for Jupyter cells
    std::string get_tmp_prefix();
    std::string get_tmp_suffix();
    std::string get_cell_tmp_file(const std::string& content);
}

#endif