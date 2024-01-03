/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 * Copyright (c) 2023, Johan Mabille, Loic Gouarin, Sylvain Corlay, Wolf Vollprecht *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
  *                                                                                 *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/


#ifndef XEUS_CPP_INTERPRETER_WASM_HPP
#define XEUS_CPP_INTERPRETER_WASM_HPP

#include "xinterpreter.hpp"
#include "xeus_cpp_config.hpp"

namespace xcpp
{
    class XEUS_CPP_API wasm_interpreter : public interpreter
    {
    public:

        wasm_interpreter();
        virtual ~wasm_interpreter() = default;

    };
}

#endif
