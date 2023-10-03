//===------- pythonexec.hpp - Python/C++ Interoperability -------*- C++ -*-===//
//
// Licensed under the Apache License v2.0.
// SPDX-License-Identifier: Apache-2.0
//
// The full license is in the file LICENSE, distributed with this software.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the Python/C++ interoperability in which two cells within
//  the same notebook can be in a either language.
//
//===----------------------------------------------------------------------===//

#ifndef XMAGICS_PYTHONEXEC_HPP
#define XMAGICS_PYTHONEXEC_HPP

#include "xeus-cpp/xbuffer.hpp"
#include "xeus-cpp/xinterpreter.hpp"
#include "xeus-cpp/xmagics.hpp"
#include "xeus-cpp/xoptions.hpp"

#include <cstddef>
#include <string>

namespace xcpp {

    class pythonexec: public xmagic_cell
    {
    public:

        argparser get_options();
        virtual void operator()(const std::string& line, const std::string& cell) override {
          std::string cline = line;
          std::string ccell = cell;
          execute(cline, ccell);
        };

        static void update_python_dict_var(const char *name, int value);
        static void update_python_dict_var_vector(const char *name,
                                            std::vector<int> &data);
        void check_python_globals();
        void exec_python_simple_command(const std::string code);
        static std::string transfer_python_ints_utility();
        static std::string transfer_python_lists_utility();
        static bool python_check_for_initialisation();

    private:

        static void startup();
        void execute(std::string &line, std::string &cell);
    };

} // namespace xcpp
#endif
