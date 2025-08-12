/***************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors
 *
 * Distributed under the terms of the BSD 3-Clause License.
 *
 * The full license is in the file LICENSE, distributed with this software.
 ****************************************************************************/

#include <iostream>
#include <memory>

#include <emscripten/bind.h>

#include <xeus/xembind.hpp>

#include "xeus-cpp/xinterpreter.hpp"

template <class interpreter_type>
static interpreter_type* builder_with_args(emscripten::val js_args)
{
    // TODO: Refactor interpreter constructor to avoid static args-to-argv conversion.
    static std::vector<std::string> args = emscripten::vecFromJSArray<std::string>(js_args);
    static std::vector<const char*> argv_vec;

    for (const auto& s : args)
    {
        argv_vec.push_back(s.c_str());
    }

    int argc = static_cast<int>(argv_vec.size());
    char** argv = const_cast<char**>(argv_vec.data());

    auto* res = new interpreter_type(argc, argv);
    argv_vec.clear();
    args.clear();
    return res;
}

EMSCRIPTEN_BINDINGS(my_module)
{
    xeus::export_core();
    using interpreter_type = xcpp::interpreter;
    xeus::export_kernel<interpreter_type, &builder_with_args<interpreter_type>>("xkernel");
}
