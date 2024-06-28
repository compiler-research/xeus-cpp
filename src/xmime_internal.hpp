/************************************************************************************
 * Copyright (c) 2016, Johan Mabille, Loic Gouarin, Sylvain Corlay, Wolf Vollprecht *
 * Copyright (c) 2016, QuantStack                                                   *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#ifndef XCPP_MIME_INTERNAL_HPP
#define XCPP_MIME_INTERNAL_HPP

#include <cstddef>
#include <locale>
#include <string>

#include "clang/Interpreter/CppInterOp.h"  // from CppInterOp package

#include <nlohmann/json.hpp>

namespace nl = nlohmann;

namespace xcpp
{
    inline nl::json mime_repr(intptr_t V)
    {
        // Return a JSON mime bundle representing the specified value.
        void* value_ptr = reinterpret_cast<void*>(V);

        // Include "xcpp/xmime.hpp" only on the first time a variable is displayed.
        static bool xmime_included = false;

        if (!xmime_included)
        {
            Cpp::Declare("#include \"xcpp/xmime.hpp\"");
            xmime_included = true;
        }

        intptr_t mimeReprV;
        bool hadError = false;
        {
            std::ostringstream code;
            code << "using xcpp::mime_bundle_repr;";
            code << "mime_bundle_repr(";
            // code << "*(" << getTypeAsString(V);
            code << &value_ptr;
            code << "));";

            std::string codeString = code.str();

            mimeReprV = Cpp::Evaluate(codeString.c_str(), &hadError);
        }

        void* mimeReprV_ptr = reinterpret_cast<void*>(V);

        if (!hadError && mimeReprV_ptr)
        {
            return *(nl::json*) mimeReprV_ptr;
        }
        else
        {
            return nl::json::object();
        }
    }
}

#endif
