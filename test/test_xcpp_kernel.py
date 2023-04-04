#############################################################################
# Copyright (c) 2023, xeus-cpp contributors
#
# Distributed under the terms of the BSD 3-Clause License.
#
# The full license is in the file LICENSE, distributed with this software.
#############################################################################


##########################################################################################
# TODO
# Please remove fake test and uncomment the code lines underneath it in order to enable
# the actual tests (to be modified/adapted)
#########################################################################################
# These tests examples (to be modified) check that the given code "hello, world" would be
# published as text stdout stream message and "error" as stderr
# Other tests are performed during the initialization of `jupyter_kernel_test.KernelTests`
# and inside the `execute_helper` function based on the given variables defined within
# the class
#########################################################################################
#                               TO BE REMOVED                                           #
#########################################################################################

import unittest

class TestFake(unittest.TestCase):

    def test_fake(self):
        pass

#########################################################################################
#                            TO UNCOMMENT AND ADAPT                                     #
#########################################################################################

#import tempfile
#import unittest
#import jupyter_kernel_test


#class KernelTests(jupyter_kernel_test.KernelTests):

    #kernel_name = "xcpp"
    #language_name = "cpp"
    #code_hello_world = "hello, world"
    #code_page_something = "?"
    #completion_samples = [{"text": "H", "matches": {"Hello", "Hey", "Howdy"}}]
    #complete_code_samples = ["hello, world"]
    #incomplete_code_samples = ["incomplete"]
    #invalid_code_samples = ["invalid"]
    #code_inspect_sample = "print"

    #def test_stdout(self):
        #self.flush_channels()
        #reply, output_msgs = self.execute_helper(code="hello, world")
        #self.assertEqual(output_msgs[0]["msg_type"], "stream")
        #self.assertEqual(output_msgs[0]["content"]["name"], "stdout")
        #self.assertEqual(output_msgs[0]["content"]["text"], "hello, world")

    #def test_stderr(self):
        #self.flush_channels()
        #reply, output_msgs = self.execute_helper(code="error")
        #self.assertEqual(output_msgs[0]["msg_type"], "stream")
        #self.assertEqual(output_msgs[0]["content"]["name"], "stderr")

#########################################################################################
#########################################################################################

if __name__ == "__main__":
    unittest.main()
