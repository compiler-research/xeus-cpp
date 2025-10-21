"""utilities for testing xeus-cpp kernels"""

from __future__ import annotations

import atexit
import os
import sys
from contextlib import contextmanager
from queue import Empty
from subprocess import STDOUT
from tempfile import TemporaryDirectory
from time import time

from jupyter_client import manager
from jupyter_client.blocking.client import BlockingKernelClient

STARTUP_TIMEOUT = 60
TIMEOUT = 100

KM: manager.KernelManager = None  # type:ignore
KC: BlockingKernelClient = None  # type:ignore


def start_new_kernel(**kwargs):
    """start a new xeus-cpp kernel, and return its Manager and Client
    
    Integrates with our output capturing for tests.
    """
    kwargs["stderr"] = STDOUT
    # Specify xeus-cpp kernel
    kwargs.setdefault("kernel_name", "xcpp17-debugger")
    return manager.start_new_kernel(startup_timeout=STARTUP_TIMEOUT, **kwargs)


def flush_channels(kc=None):
    """flush any messages waiting on the queue"""
    if kc is None:
        kc = KC
    for get_msg in (kc.get_shell_msg, kc.get_iopub_msg):
        while True:
            try:
                msg = get_msg(timeout=0.1)
            except Empty:
                break


def get_reply(kc, msg_id, timeout=TIMEOUT, channel="shell"):
    """Get a reply for a specific message ID"""
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


def get_replies(kc, msg_ids: list[str], timeout=TIMEOUT, channel="shell"):
    """Get replies which may arrive in any order"""
    count = 0
    replies = [None] * len(msg_ids)
    while count < len(msg_ids):
        get_msg = getattr(kc, f"get_{channel}_msg")
        reply = get_msg(timeout=timeout)
        try:
            msg_id = reply["parent_header"]["msg_id"]
            replies[msg_ids.index(msg_id)] = reply
            count += 1
        except ValueError:
            print(f"Ignoring reply not to any of {msg_ids}: {reply}")
    return replies


def execute(code="", kc=None, **kwargs):
    """Execute C++ code and validate the execution request
    
    Args:
        code: C++ code to execute
        kc: kernel client (uses global KC if None)
        **kwargs: additional arguments for kc.execute()
        
    Returns:
        tuple: (msg_id, reply_content)
    """
    if kc is None:
        kc = KC
    msg_id = kc.execute(code=code, **kwargs)
    reply = get_reply(kc, msg_id, TIMEOUT)
    
    # Validate it's an execute_reply
    assert reply["msg_type"] == "execute_reply", f"Expected execute_reply, got {reply['msg_type']}"
    
    busy = kc.get_iopub_msg(timeout=TIMEOUT)
    assert busy["msg_type"] == "status"
    assert busy["content"]["execution_state"] == "busy"

    if not kwargs.get("silent"):
        execute_input = kc.get_iopub_msg(timeout=TIMEOUT)
        assert execute_input["msg_type"] == "execute_input"
        assert execute_input["content"]["code"] == code

    # show tracebacks/errors if present for debugging
    if reply["content"].get("traceback"):
        print("\n".join(reply["content"]["traceback"]), file=sys.stderr)
    
    if reply["content"]["status"] == "error":
        print(f"Error: {reply['content'].get('ename', 'Unknown')}", file=sys.stderr)
        print(f"Message: {reply['content'].get('evalue', '')}", file=sys.stderr)

    return msg_id, reply["content"]


def start_global_kernel(kernel_name="xcpp17"):
    """start the global kernel (if it isn't running) and return its client
    
    Args:
        kernel_name: xeus-cpp kernel variant (xcpp11, xcpp14, xcpp17, xcpp20)
    """
    global KM, KC
    if KM is None:
        KM, KC = start_new_kernel(kernel_name=kernel_name)
        atexit.register(stop_global_kernel)
    else:
        flush_channels(KC)
    return KC


@contextmanager
def kernel(kernel_name="xcpp17"):
    """Context manager for the global kernel instance

    Should be used for most kernel tests

    Args:
        kernel_name: xeus-cpp kernel variant (xcpp11, xcpp14, xcpp17, xcpp20)

    Yields:
        kernel_client: connected KernelClient instance
    """
    yield start_global_kernel(kernel_name=kernel_name)


def uses_kernel(test_f):
    """Decorator for tests that use the global kernel"""

    def wrapped_test():
        with kernel() as kc:
            test_f(kc)

    wrapped_test.__doc__ = test_f.__doc__
    wrapped_test.__name__ = test_f.__name__
    return wrapped_test


def stop_global_kernel():
    """Stop the global shared kernel instance, if it exists"""
    global KM, KC
    if KC is not None:
        KC.stop_channels()
        KC = None  # type:ignore
    if KM is not None:
        KM.shutdown_kernel(now=True)
        KM = None  # type:ignore


@contextmanager
def new_kernel(argv=None, kernel_name="xcpp17"):
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


def assemble_output(get_msg, timeout=1, parent_msg_id: str | None = None):
    """Assemble stdout/stderr from an execution
    
    Args:
        get_msg: function to get messages (usually kc.get_iopub_msg)
        timeout: timeout for getting messages
        parent_msg_id: filter messages by parent message ID
        
    Returns:
        tuple: (stdout, stderr)
    """
    stdout = ""
    stderr = ""
    while True:
        try:
            msg = get_msg(timeout=timeout)
        except Empty:
            break
            
        msg_type = msg["msg_type"]
        content = msg["content"]

        if parent_msg_id is not None and msg["parent_header"]["msg_id"] != parent_msg_id:
            continue

        if msg_type == "status" and content["execution_state"] == "idle":
            break
        elif msg_type == "stream":
            if content["name"] == "stdout":
                stdout += content["text"]
            elif content["name"] == "stderr":
                stderr += content["text"]
            else:
                raise KeyError("bad stream: %r" % content["name"])
                
    return stdout, stderr


def wait_for_idle(kc, parent_msg_id: str | None = None, timeout=TIMEOUT):
    """Wait for kernel to become idle
    
    Args:
        kc: kernel client
        parent_msg_id: optional parent message ID to match
        timeout: maximum time to wait
    """
    start_time = time()
    while True:
        elapsed = time() - start_time
        if elapsed > timeout:
            raise TimeoutError(f"Kernel did not become idle within {timeout}s")
            
        msg = kc.get_iopub_msg(timeout=min(1, timeout - elapsed))
        msg_type = msg["msg_type"]
        content = msg["content"]
        if (
            msg_type == "status"
            and content["execution_state"] == "idle"
            and (parent_msg_id is None or msg["parent_header"]["msg_id"] == parent_msg_id)
        ):
            break


class TemporaryWorkingDirectory(TemporaryDirectory):
    """
    Creates a temporary directory and sets the cwd to that directory.
    Automatically reverts to previous cwd upon cleanup.
    
    Usage example:
        with TemporaryWorkingDirectory() as tmpdir:
            ...
    """

    def __enter__(self):
        self.old_wd = os.getcwd()
        os.chdir(self.name)
        return super().__enter__()

    def __exit__(self, exc, value, tb):
        os.chdir(self.old_wd)
        return super().__exit__(exc, value, tb)


# C++-specific helper functions

def compile_and_run(code: str, kc=None, includes: list[str] = None, 
                    flags: list[str] = None) -> tuple[str, dict]:
    """Helper to compile and run C++ code with common setup
    
    Args:
        code: C++ code to execute
        kc: kernel client
        includes: list of headers to include
        flags: compiler flags to set
        
    Returns:
        tuple: (msg_id, reply_content)
    """
    if kc is None:
        kc = KC
        
    full_code = ""
    
    # Add includes
    if includes:
        for inc in includes:
            full_code += f"#include <{inc}>\n"
        full_code += "\n"
    
    # Add compiler flags if needed
    if flags:
        for flag in flags:
            full_code += f"#pragma cling add_compiler_flag {flag}\n"
        full_code += "\n"
    
    full_code += code
    
    return execute(full_code, kc)


def check_cpp_version(kc=None) -> str:
    """Check which C++ standard is being used"""
    if kc is None:
        kc = KC
        
    code = """
    #include <iostream>
    std::cout << __cplusplus << std::endl;
    """
    
    msg_id, reply = execute(code, kc)
    stdout, _ = assemble_output(kc.get_iopub_msg, parent_msg_id=msg_id)
    
    version_map = {
        "199711": "C++98",
        "201103": "C++11",
        "201402": "C++14",
        "201703": "C++17",
        "202002": "C++20",
    }
    
    return version_map.get(stdout.strip(), f"Unknown ({stdout.strip()})")