#############################################################################
# Copyright (c) 2023, xeus-cpp contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.
#############################################################################

import unittest
import jupyter_kernel_test


class XCppTests(jupyter_kernel_test.KernelTests):

    kernel_name = 'xcpp'

    # language_info.name in a kernel_info_reply should match this
    language_name = 'C++'

    # Code that should write the exact string `hello, world` to STDOUT
    code_hello_world = '#include <iostream>\nstd::cout << "hello, world" << std::endl;'

    # Code that should cause (any) text to be written to STDERR
    code_stderr = '#include <iostream>\nstd::cerr << "oops" << std::endl;'

    # Pager: code that should display something (anything) in the pager
    #code_page_something = "?std::vector"

    # Samples of code which generate a result value (ie, some text
    # displayed as Out[n])
    #code_execute_result = [
    #    {
    #        'code': '6 * 7',
    #        'result': '42'
    #    }
    #]

    # Samples of code which should generate a rich display output, and
    # the expected MIME type
    code_display_data = [
        {
            'code': '#include <string>\n#include "xcpp/xdisplay.hpp"\nstd::string test("foobar");\nxcpp::display(test);',
            'mime': 'text/plain'
        }
    ]

if __name__ == '__main__':
    unittest.main()
