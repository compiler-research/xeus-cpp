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
import nbformat
import papermill as pm

class BaseXCppCompleteTests(jupyter_kernel_test.KernelTests):
    __test__ = False
    
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
        self.assertEqual(reply["content"]["matches"], ['float', 'foo'])
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

kernel_names = ['xcpp17', 'xcpp20', 'xcpp23','xcpp17-omp', 'xcpp20-omp', 'xcpp23-omp']

for name in kernel_names:
    class_name = f"XCppCompleteTests_{name}"
    globals()[class_name] = type(
        class_name,
        (BaseXCppCompleteTests,),
        {
            'kernel_name': name,
            '__test__': True
        }
    )

if platform.system() != 'Windows':
    class BaseXCppTests(jupyter_kernel_test.KernelTests):
        __test__ = False
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
    #include "xeus/xbase64.hpp"
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
        bundle["image/png"] = xeus::base64encode(i.m_buffer.str());
        return bundle;
      }
    }
    #include "xcpp/xdisplay.hpp"
    im::image marie("../notebooks/images/marie.png");
    xcpp::display(marie);""",
                'mime': 'image/png'
            }
        ]

    for name in kernel_names:
        class_name = f"XCppTests_{name}"
        globals()[class_name] = type(
            class_name,
            (BaseXCppTests,),
            {
                'kernel_name': name,
                '__test__': True
            }
        )

    # Tests for Notebooks
    class BaseXCppNotebookTests(unittest.TestCase):
        __test__ = False
        notebook_names = [
            'xeus-cpp'
            # Add more notebook names as needed
        ]

        def test_notebooks(self):
            for name in self.notebook_names:

                inp = f'Notebooks/{name}.ipynb'
                out = f'Notebooks/{name}_output.ipynb'

                with open(inp) as f:
                    input_nb = nbformat.read(f, as_version=4)

                try:
                    # Execute the notebook
                    executed_notebook = pm.execute_notebook( 
                        inp, 
                        out,
                        log_output=True,
                        kernel_name=self.kernel_name
                    )

                    if executed_notebook is None:  # Explicit check for None or any other condition
                        self.fail(f"Execution of notebook {name} returned None")
                except Exception as e:
                    self.fail(f"Notebook {name} failed to execute: {e}")

                with open(out) as f:
                    output_nb = nbformat.read(f, as_version=4)

                # Iterate over the cells in the input and output notebooks
                for i, (input_cell, output_cell) in enumerate(zip(input_nb.cells, output_nb.cells)):
                    if input_cell.cell_type == 'code' and output_cell.cell_type == 'code':
                        # If one cell has output and the other doesn't, set check to False
                        if bool(input_cell.outputs) != bool(output_cell.outputs):
                            self.fail(f"Cell {i} in notebook {name} has mismatched output presence")
                        else:
                            if input_cell.outputs != output_cell.outputs:
                                self.fail(f"{input_output.get('text')} != {output_output.get('text')} Cell {i} in notebook {name} has mismatched output type")

    for name in kernel_names:
        class_name = f"XCppNotebookTests_{name}"
        globals()[class_name] = type(
            class_name,
            (BaseXCppNotebookTests,),
            {
                'kernel_name': name,
                '__test__': True
            }
        )

class BaseXCppTests2(jupyter_kernel_test.KernelTests):
    __test__ = False

    # language_info.name in a kernel_info_reply should match this
    language_name = 'C++'

    # Code that should write the exact string `hello, world` to STDOUT
    code_hello_world = '#include <stdio.h>\nprintf("hello, world");'

for name in kernel_names:
    class_name = f"XCppTests2_{name}"
    globals()[class_name] = type(
        class_name,
        (BaseXCppTests2,),
        {
            'kernel_name': name,
            '__test__': True
        }
    )

if platform.system() != 'Windows':
    class BaseXCppOpenMPTests(jupyter_kernel_test.KernelTests):
        __test__ = False

        # language_info.name in a kernel_info_reply should match this
        language_name = 'C++'

        # OpenMP test that creates 2 threads, and gets them to output their thread
        # number in descending order
        code_omp="""
        #include <omp.h>
        #include <iostream>
        """

        code_omp_2="""
        int main() {
        omp_set_num_threads(2);
        #pragma omp parallel
        {
           if (omp_get_thread_num() == 1) {
             printf("1");
             #pragma omp barrier
           }
           else if (omp_get_thread_num() == 0) {
             #pragma omp barrier
             printf("0");
           }
        }
        }
        main();
        """
    
        def test_xcpp_omp(self):
                self.flush_channels()
                reply, output_msgs = self.execute_helper(code=self.code_omp,timeout=20)
                reply, output_msgs = self.execute_helper(code=self.code_omp_2,timeout=20)
                self.assertEqual(output_msgs[0]['msg_type'], 'stream')
                self.assertEqual(output_msgs[0]['content']['text'], '10')
                self.assertEqual(output_msgs[0]['content']['name'], 'stdout')

    kernel_names = ['xcpp17-omp', 'xcpp20-omp', 'xcpp23-omp']

    for name in kernel_names:
        class_name = f"XCppOpenMPTests_{name}"
        globals()[class_name] = type(
            class_name,
            (BaseXCppOpenMPTests,),
            {
                'kernel_name': name,
                '__test__': True
            }
        )

if __name__ == '__main__':
    unittest.main()
