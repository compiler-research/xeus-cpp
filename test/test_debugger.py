"""Test xeus-cpp debugger functionality using LLDB"""

import pytest
import time
from queue import Empty
from subprocess import STDOUT
from jupyter_client import manager
from contextlib import contextmanager
from time import time

KM: manager.KernelManager = None  # type:ignore

seq = 0
TIMEOUT=100
KERNEL_NAME = "xcpp17-debugger"

def get_reply(kc, msg_id, timeout=TIMEOUT, channel="shell"):
    t0 = time()
    while True:
        get_msg = getattr(kc, f"get_{channel}_msg")
        reply = get_msg(timeout=timeout)
        if reply["parent_header"]["msg_id"] == msg_id:
            break
        # Allow debugging ignored replies
        print(f"Ignoring reply not to {msg_id}: {reply}")
        t1 = time()
        timeout -= t1 - t0
        t0 = t1
    return reply

@contextmanager
def new_kernel(argv=None, kernel_name=KERNEL_NAME):
    """Context manager for a new kernel in a subprocess

    Should only be used for tests where the kernel must not be reused.

    Args:
        argv: additional command-line arguments
        kernel_name: xeus-cpp kernel variant (xcpp11, xcpp14, xcpp17, xcpp20)

    Yields:
        kernel_client: connected KernelClient instance
    """
    kwargs = {"stderr": STDOUT, "kernel_name": kernel_name}
    if argv is not None:
        kwargs["extra_arguments"] = argv
    with manager.run_kernel(**kwargs) as kc:
        yield kc

# xeus-cpp uses LLDB for debugging
# Test if xeus-cpp kernel with debugger support is available
try:
    from jupyter_client import kernelspec
    ksm = kernelspec.KernelSpecManager()
    available_kernels = ksm.find_kernel_specs()
    print(f"Available kernels: {available_kernels}")
    HAS_XCPP_DEBUGGER = any(KERNEL_NAME in k for k in available_kernels.keys())
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

def send_initialize_request(kc):
    """Send Initialize request"""

    reply = send_debug_request(
        kc,
        "initialize",
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
        },
    )

    return reply
        

@pytest.fixture()
def kernel():
    """Create a fresh xeus-cpp kernel"""
    with new_kernel(kernel_name=KERNEL_NAME) as kc:
        # Give kernel time to fully start
        time.sleep(1)
        yield kc


def test_kernel_starts():
    """Test that kernel can start"""
    with new_kernel(kernel_name=KERNEL_NAME) as kc:
        msg_id = kc.execute("int test = 1;")
        reply = kc.get_shell_msg(timeout=TIMEOUT)
        assert reply["content"]["status"] == "ok"

def test_debug_initialize():
    """Test debugger initialization"""
    with new_kernel(kernel_name=KERNEL_NAME) as kc:
        reply = send_initialize_request(kc)
        assert reply["success"] == True


def test_debug_attach():
    """Test debugger attachment using attach method"""
    with new_kernel(kernel_name=KERNEL_NAME) as kc:
        init_reply = send_initialize_request(kc)
        assert init_reply["success"] == True
    
        attach_reply = send_debug_request(kc, "attach")
        assert attach_reply["success"] == True


def test_dump_cell():
    """Test dumping cell code"""
    with new_kernel(kernel_name=KERNEL_NAME) as kc:
        init_reply = send_initialize_request(kc)
        assert init_reply["success"] == True
    
        attach_reply = send_debug_request(kc, "attach")
        assert attach_reply["success"] == True
        
        code = """
#include <iostream>
int add(int a, int b) {
    return a + b;
}
int result = add(2, 3);
"""
        dump_reply = send_debug_request(
            kc,
            "dumpCell",
            {"code": code}
        )
        assert dump_reply["success"] == True
        
        source_path = dump_reply.get("body", {}).get("sourcePath")
        assert source_path is not None

def test_set_breakpoints():
    """Test setting breakpoint (without execution)"""
    with new_kernel(kernel_name=KERNEL_NAME) as kc:
        init_reply = send_initialize_request(kc)
        assert init_reply["success"] == True
    
        attach_reply = send_debug_request(kc, "attach")
        assert attach_reply["success"] == True
        
        code = """
#include <iostream>
int add(int a, int b) {
    return a + b;
}
int result = add(2, 3);
"""
        kc.execute(code)

        dump_reply = send_debug_request(
            kc,
            "dumpCell",
            {"code": code}
        )
        assert dump_reply["success"] == True
        
        source_path = dump_reply.get("body", {}).get("sourcePath")
        assert source_path is not None
    
        bp_reply = send_debug_request(
            kc,
            "setBreakpoints",
            {
                "source": {"path": source_path},
                "breakpoints": [{"line": 2}],
                "sourceModified": False,
            }
        )
        assert bp_reply["success"] == True

def test_stop_on_breakpoint():
    """Test setting breakpoint and hitting it during execution"""
    with new_kernel(kernel_name=KERNEL_NAME) as kc:
        init_reply = send_initialize_request(kc)
        assert init_reply["success"] == True
    
        attach_reply = send_debug_request(kc, "attach")
        assert attach_reply["success"] == True
        
        code = """
int divide(int a, int b) {
    int result = a / b;
    return result;
}
divide(10, 2);
        """

        kc.execute(code)

        dump_reply = send_debug_request(kc, "dumpCell", {"code": code})
        source_path = dump_reply.get("body", {}).get("sourcePath")
        assert source_path is not None

        bp_reply = send_debug_request(
            kc,
            "setBreakpoints",
            {
                "source": {"path": source_path},
                "breakpoints": [{"line": 2}],
                "sourceModified": False,
            }
        )
        assert bp_reply["success"] == True

        msg_id = kc.execute("divide(10, 2);")

        msg: dict = {"msg_type": "", "content": {}}
        while msg.get("msg_type") != "debug_event" or msg["content"].get("event") != "stopped":
            msg = kc.get_iopub_msg(timeout=TIMEOUT)

        assert msg["content"]["body"]["reason"] == "breakpoint"

def test_step_in():
    """Test stepping into a function"""
    with new_kernel(kernel_name=KERNEL_NAME) as kc:
        init_reply = send_initialize_request(kc)
        assert init_reply["success"] == True
    
        attach_reply = send_debug_request(kc, "attach")
        assert attach_reply["success"] == True
        
        code = """
int multiply(int a, int b) {
    return a * b;
}
int calculate(int x, int y) {
    int product = multiply(x, y);
    return product;
}
calculate(5, 3);
"""
        kc.execute(code)

        dump_reply = send_debug_request(kc, "dumpCell", {"code": code})
        source_path = dump_reply.get("body", {}).get("sourcePath")
        assert source_path is not None

        bp_reply = send_debug_request(
            kc,
            "setBreakpoints",
            {
                "source": {"path": source_path},
                "breakpoints": [{"line": 5}],
                "sourceModified": False,
            }
        )
        assert bp_reply["success"] == True

        msg_id = kc.execute("calculate(5, 3);")

        msg: dict = {"msg_type": "", "content": {}}
        while msg.get("msg_type") != "debug_event" or msg["content"].get("event") != "stopped":
            msg = kc.get_iopub_msg(timeout=TIMEOUT)

        assert msg["content"]["body"]["reason"] == "breakpoint"

        threadId = int(msg["content"]["body"]["threadId"])

        step_in_reply = send_debug_request(kc, "stepIn", {"threadId": threadId})
        assert step_in_reply["success"] == True

        msg = {"msg_type": "", "content": {}}
        while msg.get("msg_type") != "debug_event" or msg["content"].get("event") != "stopped":
            msg = kc.get_iopub_msg(timeout=TIMEOUT)

        assert msg["content"]["body"]["reason"] == "step"

def test_step_out():
    """Test stepping out of a function"""
    with new_kernel(kernel_name=KERNEL_NAME) as kc:
        init_reply = send_initialize_request(kc)
        assert init_reply["success"] == True
    
        attach_reply = send_debug_request(kc, "attach")
        assert attach_reply["success"] == True
        
        code = """
int subtract(int a, int b) {
    int result = a - b;
    return result;
}
int compute(int x, int y) {
    int difference = subtract(x, y);
    return difference;
}
compute(10, 4);
"""
        kc.execute(code)

        dump_reply = send_debug_request(kc, "dumpCell", {"code": code})
        source_path = dump_reply.get("body", {}).get("sourcePath")
        assert source_path is not None

        # Set breakpoint inside subtract function
        bp_reply = send_debug_request(
            kc,
            "setBreakpoints",
            {
                "source": {"path": source_path},
                "breakpoints": [{"line": 2}],
                "sourceModified": False,
            }
        )
        assert bp_reply["success"] == True

        msg_id = kc.execute("compute(10, 4);")

        msg: dict = {"msg_type": "", "content": {}}
        while msg.get("msg_type") != "debug_event" or msg["content"].get("event") != "stopped":
            msg = kc.get_iopub_msg(timeout=TIMEOUT)

        assert msg["content"]["body"]["reason"] == "breakpoint"

        threadId = int(msg["content"]["body"]["threadId"])

        step_out_reply = send_debug_request(kc, "stepOut", {"threadId": threadId})
        assert step_out_reply["success"] == True

        msg = {"msg_type": "", "content": {}}
        while msg.get("msg_type") != "debug_event" or msg["content"].get("event") != "stopped":
            msg = kc.get_iopub_msg(timeout=TIMEOUT)

        assert msg["content"]["body"]["reason"] == "step"

def test_continue_execution():
    """Test continuing execution after hitting breakpoint"""
    with new_kernel(kernel_name=KERNEL_NAME) as kc:
        init_reply = send_initialize_request(kc)
        assert init_reply["success"] == True
    
        attach_reply = send_debug_request(kc, "attach")
        assert attach_reply["success"] == True
        
        code = """
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}
int result = factorial(5);
"""
        kc.execute(code)

        dump_reply = send_debug_request(kc, "dumpCell", {"code": code})
        source_path = dump_reply.get("body", {}).get("sourcePath")
        assert source_path is not None

        bp_reply = send_debug_request(
            kc,
            "setBreakpoints",
            {
                "source": {"path": source_path},
                "breakpoints": [{"line": 2}],
                "sourceModified": False,
            }
        )
        assert bp_reply["success"] == True

        msg_id = kc.execute("factorial(5);")

        # Wait for breakpoint hit
        msg: dict = {"msg_type": "", "content": {}}
        while msg.get("msg_type") != "debug_event" or msg["content"].get("event") != "stopped":
            msg = kc.get_iopub_msg(timeout=TIMEOUT)

        assert msg["content"]["body"]["reason"] == "breakpoint"

        threadId = int(msg["content"]["body"]["threadId"])

        continue_reply = send_debug_request(kc, "continue", {"threadId": threadId})
        assert continue_reply["success"] == True

def test_stack_trace():
    """Test getting stack trace at breakpoint"""
    with new_kernel(kernel_name=KERNEL_NAME) as kc:
        init_reply = send_initialize_request(kc)
        assert init_reply["success"] == True
    
        attach_reply = send_debug_request(kc, "attach")
        assert attach_reply["success"] == True
        
        code = """
int inner(int x) {
    return x * 2;
}
int outer(int y) {
    return inner(y + 1);
}
outer(5);
"""
        kc.execute(code)

        dump_reply = send_debug_request(kc, "dumpCell", {"code": code})
        source_path = dump_reply.get("body", {}).get("sourcePath")
        assert source_path is not None

        bp_reply = send_debug_request(
            kc,
            "setBreakpoints",
            {
                "source": {"path": source_path},
                "breakpoints": [{"line": 2}],
                "sourceModified": False,
            }
        )
        assert bp_reply["success"] == True

        msg_id = kc.execute("outer(5);")

        msg: dict = {"msg_type": "", "content": {}}
        while msg.get("msg_type") != "debug_event" or msg["content"].get("event") != "stopped":
            msg = kc.get_iopub_msg(timeout=TIMEOUT)

        assert msg["content"]["body"]["reason"] == "breakpoint"

        threadId = int(msg["content"]["body"]["threadId"])

        stacktrace_reply = send_debug_request(kc, "stackTrace", {"threadId": threadId})
        assert stacktrace_reply["success"] == True
        assert len(stacktrace_reply.get("body", {}).get("stackFrames", [])) > 0

def test_inspect_variables():
    """Test inspecting variables at breakpoint"""
    with new_kernel(kernel_name=KERNEL_NAME) as kc:
        init_reply = send_initialize_request(kc)
        assert init_reply["success"] == True
    
        attach_reply = send_debug_request(kc, "attach")
        assert attach_reply["success"] == True
        
        code = """
int process(int a, int b) {
    int sum = a + b;
    int product = a * b;
    return sum + product;
}
process(3, 4);
"""
        kc.execute(code)

        dump_reply = send_debug_request(kc, "dumpCell", {"code": code})
        source_path = dump_reply.get("body", {}).get("sourcePath")
        assert source_path is not None

        bp_reply = send_debug_request(
            kc,
            "setBreakpoints",
            {
                "source": {"path": source_path},
                "breakpoints": [{"line": 3}],
                "sourceModified": False,
            }
        )
        assert bp_reply["success"] == True

        msg_id = kc.execute("process(3, 4);")

        # Wait for breakpoint hit
        msg: dict = {"msg_type": "", "content": {}}
        while msg.get("msg_type") != "debug_event" or msg["content"].get("event") != "stopped":
            msg = kc.get_iopub_msg(timeout=TIMEOUT)

        assert msg["content"]["body"]["reason"] == "breakpoint"

        threadId = int(msg["content"]["body"]["threadId"])

        # Get scopes
        stacktrace_reply = send_debug_request(kc, "stackTrace", {"threadId": threadId})
        frame_id = stacktrace_reply["body"]["stackFrames"][0]["id"]
        
        scopes_reply = send_debug_request(kc, "scopes", {"frameId": frame_id})
        assert scopes_reply["success"] == True
        
        # Get variables
        if scopes_reply["body"]["scopes"]:
            var_ref = scopes_reply["body"]["scopes"][0]["variablesReference"]
            vars_reply = send_debug_request(kc, "variables", {"variablesReference": var_ref})
            assert vars_reply["success"] == True

if __name__ == "__main__":
    pytest.main([__file__, "-v", "-s"])
