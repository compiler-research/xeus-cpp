Debugging and Testing
--------------------

Debugging
========================

These steps are performed using the GNU Debugger (GDB), so ensure it is installed in your environment. Then execute the command to build the project.

.. code-block:: bash

    cmake -D CMAKE_BUILD_TYPE=Debug ..

In the same folder, run the command and copy the JSON displayed in the terminal.

.. code-block:: bash

    xcpp

Save the JSON in a new file in any folder. Before proceeding to the next step, ensure that Jupyter Console is installed in the environment. Open a new terminal and run the command in the folder where the JSON file is located.

.. code-block:: bash

    jupyter console --existing yourJSONFile.json

Open a new terminal, navigate to the build directory, and run GDB to start debugging.

.. code-block:: bash

    gdb xcpp


Testing
========================

The testing code for the source files is located in `test/test_interpreter.cpp`. Write the necessary tests and build the project as described in the repository's README or contributing guidelines. Then, execute `build/test/test_xeus_cpp` to verify if the tests were successful.