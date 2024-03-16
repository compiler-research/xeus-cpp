//===------------ pythonexec.hpp - Python/C++ Interoperability ------------===//
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

#include "Python.h"

#include "pythonexec.hpp"
#include "../xparser.hpp"

#include <cstddef>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace xcpp {
static PyObject *gMainDict = 0;

void pythonexec::startup() {
    static bool isInitialized = false;
    if (isInitialized)
        return;

    Py_Initialize();

    PyRun_SimpleString("import sys\nprint(sys.path)");

    // Import cppyy module
    PyObject* cppyyModule = PyImport_ImportModule("cppyy");
    if (!cppyyModule) {
        PyErr_Print();
        Py_Finalize();
        return; // Handle import error as needed
    }

    PyObject* mainModule = PyImport_AddModule("__main__");
    PyObject_SetAttrString(mainModule, "cppyy", cppyyModule);
    Py_XDECREF(cppyyModule);
    isInitialized = true;
    // gMainDict = PyModule_GetDict(mainModule);
    // Py_INCREF(gMainDict);
    // if (!gMainDict)
    //   printf("Could not add module __main__");

    // // Retrieve the dictionary of cppyy module
    // PyObject* cppyyDict = PyModule_GetDict(cppyyModule);
    // Py_DECREF(cppyyModule);
    // if (!cppyyDict) {
    //     PyErr_Print();
    //     Py_Finalize();
    //     return; // Handle retrieval error as needed
    // }

    // // Add cppyyDict to gMainDict (if needed for further usage)
    // PyDict_Update(gMainDict, cppyyDict);

    // Py_DECREF(cppyyDict);
    // PyRun_SimpleString("import cppyy");
}

void pythonexec::get_options(argparser &argpars) {
}

void pythonexec::execute(std::string &line, std::string &cell) {
  // std::istringstream iss(line);
  // std::vector<std::string> results((std::istream_iterator<std::string>(iss)),
  //                          std::istream_iterator<std::string>());
  startup();

//  argparser argpars{"python", "Start executing Python Cell"};
//  auto argpars = get_options();
//  argpars.parse(line);

  std::string code;

  code += cell;
  if (trim(code).empty())
    return;
  // PyRun_SimpleString(
  //     "globals_copy_lists = "
  //     "globals().copy()\nfirst_dict_ints={k:globals_copy_lists[k] for k in "
  //     "set(globals_copy_lists) if type(globals_copy_lists[k]) == "
  //     "int}\nfirst_dict_lists={k:globals_copy_lists[k] for k in "
  //     "set(globals_copy_lists) if type(globals_copy_lists[k]) == list}");

  // PyRun_SimpleString("tmp = globals().copy()\nvars = [f'int {k} = {v};' for
  // k,v in tmp.items() if type(v) == int and not k.startswith('_') and k!='tmp'
  // and k!='In' and k!='Out' and k!='sys' and not hasattr(v,
  // '__call__')]\nprint(vars)"); PyRun_SimpleString("b =
  // globals().copy()\nnew_ints = ' '.join([f'int {k} = {b[k]};' for k in set(b)
  // - set(first_dict) if type(b[k]) == int])\nprint('new_ints: ', new_ints)");

  Cpp::BeginStdStreamCapture(Cpp::kStdErr);
  Cpp::BeginStdStreamCapture(Cpp::kStdOut);

  PyRun_SimpleString(code.c_str());

  std::cout << Cpp::EndStdStreamCapture();
  std::cerr << Cpp::EndStdStreamCapture();

  //   PyObject* objectsRepresentation = PyObject_Repr(gMainDict);
  //   const char* s = PyUnicode_AsUTF8(objectsRepresentation);
  //   printf("REPR of global dict: %s\n", s);
}

void pythonexec::update_python_dict_var(const char *name, int value) {
  if (!gMainDict)
    startup();
  PyObject *s;
  s = PyLong_FromLong(value);
  PyDict_SetItemString(gMainDict, name, s);
  Py_DECREF(s);
}

void pythonexec::update_python_dict_var_vector(const char *name,
                                               std::vector<int> &data) {
  if (!gMainDict)
    startup();
  PyObject *listObj = PyList_New(data.size());
  if (!listObj)
    throw std::logic_error("Unable to allocate memory for Python list");
  for (unsigned int i = 0; i < data.size(); i++) {
    PyObject *num = PyLong_FromLong((int)data[i]);
    if (!num) {
      Py_DECREF(listObj);
      throw std::logic_error("Unable to allocate memory for Python list");
    }
    PyList_SET_ITEM(listObj, i, num);
  }
  PyDict_SetItemString(gMainDict, name, listObj);
}

// check python globals
void pythonexec::check_python_globals() {
  if (!Py_IsInitialized())
    return;
  // execute the command
  PyRun_SimpleString("print(globals())");
}

// execute a python comand
void pythonexec::exec_python_simple_command(const std::string code) {
  if (!Py_IsInitialized())
    return;
  // execute the command
  PyRun_SimpleString(code.c_str());
}

std::string pythonexec::transfer_python_ints_utility() {
  if (!Py_IsInitialized())
    return " ";
  // transfer ints utility
  PyRun_SimpleString(
      "def getNewInts():\n    glob_ints_utils = globals().copy()\n    new_ints "
      "= ' '.join([f'int {k} = {glob_ints_utils[k]};' for k in "
      "set(glob_ints_utils) - set(first_dict_ints) if type(glob_ints_utils[k]) "
      "== int])\n    return new_ints");
  PyObject *ints_result =
      PyObject_CallFunction(PyDict_GetItemString(gMainDict, "getNewInts"), 0);
  if (!ints_result) {
    printf("Could not retrieve Python integers!\n");
    return " ";
  } else {
    std::string newPythonInts = PyUnicode_AsUTF8(ints_result);
    // printf("new ints %s\n", PyUnicode_AsUTF8(ints_result));
    Py_DECREF(ints_result);
    return newPythonInts;
  }
}

std::string pythonexec::transfer_python_lists_utility() {
  if (!Py_IsInitialized())
    return " ";
  // transfer lists utility
  PyRun_SimpleString("def getNewLists():\n    l = globals().copy()\n    "
                     "new_lists = ' '.join([f'int {k}() = {l[k]};' for k in "
                     "set(l) - set(first_dict_lists) if type(l[k]) == "
                     "list]).replace('[','{').replace(']','}').replace('(','[')"
                     ".replace(')',']')\n    return new_lists");
  PyObject *lists_result =
      PyObject_CallFunction(PyDict_GetItemString(gMainDict, "getNewLists"), 0);
  if (!lists_result) {
    printf("Could not retrieve Python lists!\n");
    return " ";
  } else {
    std::string newPythonLists = PyUnicode_AsUTF8(lists_result);
    Py_DECREF(lists_result);
    return newPythonLists;
  }
}

bool pythonexec::python_check_for_initialisation() {
  if (!Py_IsInitialized())
    return false;
  return true;
}
} // namespace xcpp
