/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 * Copyright (c) 2023, Martin Vassilev                                              *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#include "xeus/xinterpreter.hpp"
#include "xeus/xsystem.hpp"

#include "xeus-cpp/xinterpreter.hpp"
#include "xeus-cpp/xinterpreter_wasm.hpp"

namespace xcpp
{

    wasm_interpreter::wasm_interpreter()
        : interpreter(0, nullptr)
    {
    }
}
