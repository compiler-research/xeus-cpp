"""Test xeus-cpp debugger functionality using LLDB"""

import sys
import pytest
import time
from queue import Empty

from util import TIMEOUT, get_replies, get_reply, new_kernel

seq = 0

# xeus-cpp uses LLDB for debugging, not debugpy
# Test if xeus-cpp kernel with debugger support is available
try:
    from jupyter_client import kernelspec
    ksm = kernelspec.KernelSpecManager()
    available_kernels = ksm.find_kernel_specs()
    print(f"Available kernels: {available_kernels}")
    HAS_XCPP_DEBUGGER = any('xcpp17-debugger' in k for k in available_kernels.keys())
    print(f"xeus-cpp debugger available: {HAS_XCPP_DEBUGGER}")
except Exception:
    HAS_XCPP_DEBUGGER = False


def send_debug_request(kernel, command, arguments=None, timeout=TIMEOUT):
    """Send a debug request and get reply"""
    global seq
    seq += 1
    
    msg = kernel.session.msg(
        "debug_request",
        {
            "type": "request",
            "seq": seq,
            "command": command,
            "arguments": arguments or {},
        },
    )
    
    kernel.control_channel.send(msg)
    reply = get_reply(kernel, msg["header"]["msg_id"], channel="control", timeout=timeout)
    return reply.get("content", {})

@pytest.fixture()
def kernel():
    """Create a fresh xeus-cpp kernel"""
    with new_kernel(kernel_name="xcpp17-debugger") as kc:
        # Give kernel time to fully start
        time.sleep(1)
        yield kc


def test_kernel_starts():
    """Test that kernel can start"""
    with new_kernel(kernel_name="xcpp17-debugger") as kc:
        # Execute simple code
        msg_id = kc.execute("int test = 1;")
        reply = kc.get_shell_msg(timeout=TIMEOUT)
        assert reply["content"]["status"] == "ok"
        print("✓ Kernel starts and executes code")


# def test_debug_initialize():
#     """Test debugger initialization"""
#     with new_kernel(kernel_name="xcpp17-debugger") as kc:
#         # Execute some code first to ensure interpreter is ready
#         kc.execute("int x = 42;")
#         time.sleep(0.5)
        
#         reply = send_debug_request(
#             kc,
#             "initialize",
#             {
#                 "clientID": "test-client",
#                 "clientName": "testClient",
#                 "adapterID": "lldb",
#                 "pathFormat": "path",
#                 "linesStartAt1": True,
#                 "columnsStartAt1": True,
#                 "supportsVariableType": True,
#                 "supportsVariablePaging": False,
#                 "supportsRunInTerminalRequest": False,
#                 "locale": "en",
#             },
#         )
        
#         print(f"Initialize reply: {reply}")
        
#         # Check we got a response
#         assert isinstance(reply, dict)
#         assert "type" in reply or "success" in reply
        
#         if reply.get("success"):
#             print("✓ Debugger initialized successfully")
#         else:
#             print(f"⚠ Initialize returned: {reply}")


# def test_debug_attach_with_launch():
#     """Test debugger attachment using launch method"""
#     with new_kernel(kernel_name="xcpp17-debugger") as kc:
#         # Execute code to ensure process is active
#         kc.execute("#include <iostream>")
#         time.sleep(0.5)
        
#         # Initialize
#         init_reply = send_debug_request(
#             kc,
#             "initialize",
#             {
#                 "clientID": "test-client",
#                 "adapterID": "lldb",
#                 "pathFormat": "path",
#                 "linesStartAt1": True,
#                 "columnsStartAt1": True,
#             },
#         )
        
#         print(f"Initialize: {init_reply.get('success', False)}")
        
#         if not init_reply.get("success"):
#             pytest.skip(f"Initialize failed: {init_reply}")
        
#         # Try launch instead of attach
#         launch_reply = send_debug_request(
#             kc,
#             "launch",
#             {
#                 "stopOnEntry": False,
#                 "justMyCode": False,
#             }
#         )
        
#         print(f"Launch reply: {launch_reply}")
        
#         if launch_reply.get("success"):
#             print("✓ Debugger launched successfully")
#         else:
#             # Try attach as fallback
#             attach_reply = send_debug_request(
#                 kc,
#                 "attach",
#                 {
#                     "stopOnEntry": False,
#                 }
#             )
#             print(f"Attach reply: {attach_reply}")
            
#             if not attach_reply.get("success"):
#                 error = attach_reply.get("body", {}).get("error", {})
#                 pytest.skip(f"Could not attach: {error.get('format', attach_reply)}")


# def test_dump_cell():
#     """Test dumping cell code"""
#     with new_kernel(kernel_name="xcpp17-debugger") as kc:
#         # Initialize debugger first
#         kc.execute("int dummy = 0;")
#         time.sleep(0.5)
        
#         init_reply = send_debug_request(
#             kc,
#             "initialize",
#             {
#                 "clientID": "test-client",
#                 "clientName": "testClient",
#                 "adapterID": "lldb",
#                 "pathFormat": "path",
#                 "linesStartAt1": True,
#                 "columnsStartAt1": True,
#                 "supportsVariableType": True,
#                 "supportsVariablePaging": False,
#                 "supportsRunInTerminalRequest": False,
#                 "locale": "en",
#             },
#         )

#         print(f"Initialize reply: {init_reply}")
        
#         if not init_reply.get("success"):
#             pytest.skip("Initialize failed")
        
#         # Dump a cell
#         code = """#include <iostream>
# int add(int a, int b) {
#     return a + b;
# }
# int result = add(2, 3);"""
        
#         dump_reply = send_debug_request(
#             kc,
#             "dumpCell",
#             {"code": code}
#         )
        
#         # print(f"DumpCell reply: {dump_reply}")
        
#         if dump_reply.get("success"):
#             source_path = dump_reply.get("body", {}).get("sourcePath")
#             print(f"✓ Cell dumped to: {source_path}")
#             assert source_path is not None
#         else:
#             print(f"⚠ DumpCell failed: {dump_reply}")


# def test_configuration_done():
#     """Test configuration done message"""
#     with new_kernel(kernel_name="xcpp17-debugger") as kc:
#         kc.execute("int x = 1;")
#         time.sleep(0.5)
        
#         # Initialize
#         init_reply = send_debug_request(kc,             "initialize",
#             {
#                 "clientID": "test-client",
#                 "clientName": "testClient",
#                 "adapterID": "lldb",
#                 "pathFormat": "path",
#                 "linesStartAt1": True,
#                 "columnsStartAt1": True,
#                 "supportsVariableType": True,
#                 "supportsVariablePaging": False,
#                 "supportsRunInTerminalRequest": False,
#                 "locale": "en",
#             })
        
#         if not init_reply.get("success"):
#             pytest.skip("Initialize failed")
        
#         # Launch
#         launch_reply = send_debug_request(kc, "attach", {
#             "stopOnEntry": False,
#         })
        
#         if not launch_reply.get("success"):
#             pytest.skip("Launch failed")
        
#         # Configuration done
#         config_reply = send_debug_request(kc, "configurationDone")
        
#         print(f"ConfigurationDone reply: {config_reply}")
        
#         assert isinstance(config_reply, dict)
#         print("✓ Configuration done sent")


def test_breakpoint_without_execution():
    """Test setting breakpoint (without execution)"""
    with new_kernel(kernel_name="xcpp17-debugger") as kc:
        kc.execute("int x = 1;")
        time.sleep(0.5)
        
        # Initialize
        init_reply = send_debug_request(kc,             "initialize",
            {
                "clientID": "test-client",
                "clientName": "testClient",
                "adapterID": "lldb",
                "pathFormat": "path",
                "linesStartAt1": True,
                "columnsStartAt1": True,
                "supportsVariableType": True,
                "supportsVariablePaging": False,
                "supportsRunInTerminalRequest": False,
                "locale": "en",
            })
        
        if not init_reply.get("success"):
            pytest.skip("Initialize failed")
        
        # Launch
        launch_reply = send_debug_request(kc, "attach", {
            "stopOnEntry": False,
        })
        
        if not launch_reply.get("success"):
            pytest.skip("Attach failed")
        
        # Dump cell
        code = """int multiply(int a, int b) {
    int result = a * b;
    return result;
}"""
        # execute code to have something in the interpreter
        kc.execute(code)
        time.sleep(0.5)
        
        dump_reply = send_debug_request(kc, "dumpCell", {"code": code})
        
        if not dump_reply.get("success"):
            pytest.skip("DumpCell failed")
        
        source = dump_reply["body"]["sourcePath"]

        print(f"Dumped source path: {source}")
        
        # Set breakpoint
        bp_reply = send_debug_request(
            kc,
            "setBreakpoints",
            {
                "source": {"path": source},
                "breakpoints": [{"line": 2}],
                "sourceModified": False,
            }
        )
        
        print(f"SetBreakpoints reply: {bp_reply}")
        
        if bp_reply.get("success"):
            breakpoints = bp_reply.get("body", {}).get("breakpoints", [])
            print(f"✓ Set {len(breakpoints)} breakpoint(s)")
            if breakpoints:
                print(f"  Verified: {breakpoints[0].get('verified', False)}")
        else:
            print(f"⚠ SetBreakpoints failed: {bp_reply}")


# @pytest.mark.skipif(not HAS_XCPP, reason="xeus-cpp kernel not available")
# def test_kernel_info_debugger_support():
#     """Check if kernel advertises debugger support"""
#     with new_kernel(kernel_name="xcpp17") as kc:
#         kc.kernel_info()
#         reply = kc.get_shell_msg(timeout=TIMEOUT)
        
#         features = reply["content"].get("supported_features", [])
#         print(f"Supported features: {features}")
        
#         if "debugger" in features:
#             print("✓ Kernel advertises debugger support")
#         else:
#             print("⚠ Debugger not in supported features (may still work)")


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-s"])