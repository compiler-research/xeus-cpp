..  Copyright (c) 2023, xeus-cpp contributors 

   Distributed under the terms of the BSD 3-Clause License.  

   The full license is in the file LICENSE, distributed with this software.

Build and configuration
=======================

General Build Options
---------------------

Building the xeus-cpp library
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``xeus-cpp`` build supports the following options:

- ``XEUS_CPP_BUILD_SHARED``: Build the ``xeus-cpp`` shared library. **Enabled by default**.
- ``XEUS_CPP_BUILD_STATIC``: Build the ``xeus-cpp`` static library. **Enabled by default**.


- ``XEUS_CPP_USE_SHARED_XEUS``: Link with a `xeus` shared library (instead of the static library). **Enabled by default**.

Building the kernel
~~~~~~~~~~~~~~~~~~~

The package includes two options for producing a kernel: an executable ``xcpp`` and a Python extension module, which is used to launch a kernel from Python.

- ``XEUS_CPP_BUILD_EXECUTABLE``: Build the ``xcpp``  executable. **Enabled by default**.


If ``XEUS_CPP_USE_SHARED_XEUS_CPP`` is disabled, xcpp  will be linked statically with ``xeus-cpp``.

Building the Tests
~~~~~~~~~~~~~~~~~~

- ``XEUS_CPP_BUILD_TESTS ``: enables the tets  **Disabled by default**.

