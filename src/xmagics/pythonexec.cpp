/************************************************************************************
 * Copyright (c) 2025, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#include "pythonexec.hpp"
#include "../xparser.hpp"
#include <string>

#include "Python.h"

#include "clang/Interpreter/CppInterOp.h"

namespace xcpp {
    bool pythonexec::is_initalized = false;

    void pythonexec::operator()([[maybe_unused]] const std::string& line, const std::string& cell) {

        if (!is_initalized)
            initialize();
        if (!is_initalized) {
            // initializing failed    
            std::cout << Cpp::EndStdStreamCapture();
            std::cerr << Cpp::EndStdStreamCapture();

            std::cerr << "Failed to Initialize Python\n";
            return;
        }
        
        std::string code = trim(cell);
        if (code.empty())
            return;

        Cpp::BeginStdStreamCapture(Cpp::kStdErr);
        Cpp::BeginStdStreamCapture(Cpp::kStdOut);
        
        PyRun_SimpleString(code.c_str());

        std::cout << Cpp::EndStdStreamCapture();
        std::cerr << Cpp::EndStdStreamCapture();
    }

    void pythonexec::initialize() {
#ifdef EMSCRIPTEN
        PyStatus status;

        PyConfig config;
        PyConfig_InitPythonConfig(&config);
        static const std::wstring prefix(L"/");
        config.base_prefix = const_cast<wchar_t*>(prefix.c_str());
        config.base_exec_prefix = const_cast<wchar_t*>(prefix.c_str());
        config.prefix = const_cast<wchar_t*>(prefix.c_str());
        config.exec_prefix = const_cast<wchar_t*>(prefix.c_str());

        status = Py_InitializeFromConfig(&config);
        if (PyStatus_Exception(status)) {
            // TODO: initialization failed, propagate error
            PyConfig_Clear(&config);
            is_initalized = false;
            return;
        }
        PyConfig_Clear(&config);
#else
        Py_Initialize();
#endif

        PyRun_SimpleString("import sys\nsys.path.append('')"); // add current directory to PYTHONPATH

        // // Import cppyy module
        // PyObject* cppyyModule = PyImport_ImportModule("cppyy");
        // if (!cppyyModule) {
        //     PyErr_Print();
        //     Py_Finalize();
        //     return; // Handle import error as needed
        // }

        // PyObject* mainModule = PyImport_AddModule("__main__");
        // PyObject_SetAttrString(mainModule, "cppyy", cppyyModule);
        // Py_XDECREF(cppyyModule);

        is_initalized = true;
    }
} // namespace xcpp
