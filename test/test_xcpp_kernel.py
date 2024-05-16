#############################################################################
# Copyright (c) 2023, xeus-cpp contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.
#############################################################################

import unittest
import jupyter_kernel_test
import platform


class XCppCompleteTests(jupyter_kernel_test.KernelTests):

    kernel_name = 'xcpp20'

    # language_info.name in a kernel_info_reply should match this
    language_name = 'C++'

    # Code complete
    code_complete_presample_code = 'int foo = 12;'
    code_complete_sample = 'f'

    def test_codecomplete(self) -> None:
        if not self.code_complete_sample:
            raise SkipTest("No code complete sample")
        if self.code_complete_presample_code:
            self.flush_channels()
            reply, output_msgs = self.execute_helper(code=self.code_complete_presample_code)
            self.assertEqual(reply["content"]["status"], "ok")
        self.flush_channels()
        msg_id = self.kc.complete(self.code_complete_sample, len(self.code_complete_sample))
        reply = self.get_non_kernel_info_reply(timeout=1)
        assert reply is not None
        self.assertEqual(reply["msg_type"], "complete_reply")
        if platform.system() == 'Windows':
            self.assertEqual(str(reply["content"]["matches"]), "['fabs', 'fabsf', 'fabsl', 'float', 'foo']")
        else:
            self.assertEqual(str(reply["content"]["matches"]), "['float', 'foo']")
        self.assertEqual(reply["content"]["status"], "ok")

    # Continuation
    code_continuation_incomplete = '  int foo = 12; \\\n  float bar = 1.5f;\\'
    code_continuation_complete =   '  int foo = 12; \\\n  float bar = 1.5f;'

    def test_continuation(self) -> None:
        if not self.code_continuation_incomplete or not self.code_continuation_complete:
            raise SkipTest("No code continuation sample")

        # Incomplete
        self.flush_channels()
        msg_id = self.kc.is_complete(self.code_continuation_incomplete)
        reply = self.get_non_kernel_info_reply(timeout=1)
        assert reply is not None
        self.assertEqual(reply["msg_type"], "is_complete_reply")
        self.assertEqual(str(reply["content"]["indent"]), "  ")
        self.assertEqual(reply["content"]["status"], "incomplete")

        # Complete
        self.flush_channels()
        msg_id = self.kc.is_complete(self.code_continuation_complete)
        reply = self.get_non_kernel_info_reply(timeout=1)
        assert reply is not None
        self.assertEqual(reply["msg_type"], "is_complete_reply")
        self.assertEqual(str(reply["content"]["indent"]), "")
        self.assertEqual(reply["content"]["status"], "complete")


if platform.system() != 'Windows':
    class XCppTests(jupyter_kernel_test.KernelTests):

        kernel_name = 'xcpp20-omp'

        # language_info.name in a kernel_info_reply should match this
        language_name = 'C++'

        # Code that should write the exact string `hello, world` to STDOUT
        code_hello_world = '#include <iostream>\nstd::cout << "hello, world" << std::endl;'

        # Code that should cause (any) text to be written to STDERR
        code_stderr = '#include <iostream>\nstd::cerr << "oops" << std::endl;'

        # Pager: code that should display something (anything) in the pager
        code_page_something = "?std::vector"

        # Exception throwing
        # TODO: Remove 'if' when test work on MacOS/arm64. Throw Exceptions make
        # kernel/test non-workable.
        ###code_generate_error = 'throw std::runtime_error("Unknown exception");' if platform.system() != "Darwin" or platform.processor() != 'arm' else ''

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
            },
            {
                'code': """
    #include <string>
    #include <fstream>
    #include "nlohmann/json.hpp"
    #include "xtl/xbase64.hpp"
    namespace im {
      struct image {
        inline image(const std::string& filename) {
          std::ifstream fin(filename, std::ios::binary);
          m_buffer << fin.rdbuf();
        }
        std::stringstream m_buffer;
      };
      nlohmann::json mime_bundle_repr(const image& i) {
        auto bundle = nlohmann::json::object();
        bundle["image/png"] = xtl::base64encode(i.m_buffer.str());
        return bundle;
      }
    }
    #include "xcpp/xdisplay.hpp"
    im::image marie("../notebooks/images/marie.png");
    xcpp::display(marie);""",
                'mime': 'image/png'
            }
        ]
        code_omp="""
    #include <omp.h>
    #include <iostream>
    #include <clang/Interpreter/CppInterOp.h>  
    #include "llvm/Support/FileSystem.h"
    #include "llvm/Support/Path.h"
    std::string BinaryPath = GetExecutablePath(/*Argv0=*/nullptr);
    llvm::StringRef Dir = llvm::sys::path::parent_path(BinaryPath);
    Cpp::AddSearchPath(Dir.str().c_str());
    #ifdef __APPLE__
      std::string PathToTestSharedLib = Cpp::SearchLibrariesForSymbol("_omp_get_max_threads_", /*system_search=*/false);
    #else
      std::string PathToTestSharedLib = Cpp::SearchLibrariesForSymbol("omp_get_max_threads_", /*system_search=*/false);
    #endif // __APPLE__
    bool loaded=Cpp::LoadLibrary("PathToTestSharedLib.c_str())");
    std::cerr<<loaded<<std::endl;
    """
        def test_xcpp_omp(self):
            self.flush_channels()
            reply, output_msgs = self.execute_helper(code=self.code_omp,timeout=20)
            self.assertEqual(output_msgs[0]['msg_type'], 'stream')
            self.assertEqual(output_msgs[0]['content']['name'], 'stderr')
            self.assertEqual(output_msgs[0]['content']['text'], '2')  


class XCppTests2(jupyter_kernel_test.KernelTests):

    kernel_name = 'xcpp20'

    # language_info.name in a kernel_info_reply should match this
    language_name = 'C++'

    # Code that should write the exact string `hello, world` to STDOUT
    code_hello_world = '#include <stdio.h>\nprintf("hello, world");'


if __name__ == '__main__':
    unittest.main()
