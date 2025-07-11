/***************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors
 *
 * Distributed under the terms of the BSD 3-Clause License.
 *
 * The full license is in the file LICENSE, distributed with this software.
 ****************************************************************************/

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"

int main(int argc, char** argv) {
    doctest::Context context;

    // Set options to show more detailed test output
    context.setOption("success", true);
    context.setOption("report-failures", true);
    context.setOption("force-colors", true);
    
    context.applyCommandLine(argc, argv);

    int res = context.run();

    return res;
}
