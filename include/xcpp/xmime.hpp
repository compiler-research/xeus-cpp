/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 * Copyright (c) 2023, Johan Mabille, Loic Gouarin, Sylvain Corlay, Wolf Vollprecht *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#ifndef XCPP_MIME_HPP
#define XCPP_MIME_HPP

#include <sstream>

#include <nlohmann/json.hpp>

namespace nl = nlohmann;

namespace xcpp
{
    namespace detail
    {
        // Generic mime_bundle_repr() implementation
        // via std::ostringstream.
        template <class T>
        nl::json mime_bundle_repr_via_sstream(const T& value)
        {
            auto bundle = nl::json::object();

            std::ostringstream oss;
            oss << value;

            bundle["text/plain"] = oss.str();
            return bundle;
        }

    }

    // Default implementation of mime_bundle_repr
    template <class T>
    nl::json mime_bundle_repr(const T& value)
    {
        return detail::mime_bundle_repr_via_sstream(value);
    }
}

#endif
