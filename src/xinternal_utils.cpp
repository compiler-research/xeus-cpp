#include "xinternal_utils.hpp"

#ifdef _WIN32
#include <Windows.h>
#else
#include <cstdlib>
#endif

namespace xcpp
{
    std::string red_text(const std::string& text)
    {
#ifdef _WIN32
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
        GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
        std::string result = text;
        SetConsoleTextAttribute(hConsole, consoleInfo.wAttributes);
        return result;
#else
        return "\033[0;31m" + text + "\033[0m";
#endif
    }

    std::string green_text(const std::string& text)
    {
#ifdef _WIN32
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
        GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
        SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
        std::string result = text;
        SetConsoleTextAttribute(hConsole, consoleInfo.wAttributes);
        return result;
#else
        return "\033[0;32m" + text + "\033[0m";
#endif
    }

    std::string blue_text(const std::string& text)
    {
#ifdef _WIN32
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
        GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
        SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE);
        std::string result = text;
        SetConsoleTextAttribute(hConsole, consoleInfo.wAttributes);
        return result;
#else
        return "\033[0;34m" + text + "\033[0m";
#endif
    }

    std::string highlight(const std::string& code)
    {
        // Placeholder: No syntax highlighting implemented
        // In a real implementation, use a C++ library (e.g., libclang or a custom highlighter)
        return code;
    }

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