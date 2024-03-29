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

#include "xeus-cpp/xinterpreter_wasm.hpp"

EMSCRIPTEN_BINDINGS(my_module)
{
    xeus::export_core();
    using interpreter_type = xcpp::wasm_interpreter;
    xeus::export_kernel<interpreter_type>("xkernel");
}
