InstallationAndUsage
--------------------

You will first need to install dependencies.

.. code-block:: bash

    mamba install cmake cxx-compiler xeus-zmq nlohmann_json cppzmq xtl jupyterlab
    clangdev=16 cpp-argparse pugixml -c conda-forge


**Note:** Use a mamba environment with python version >= 3.11 for fetching clang-versions.

The safest usage is to create an environment named `xeus-cpp`.

.. code-block:: bash

    mamba create -n  xeus-cpp
    source activate  xeus-cpp

Installing from conda-forge:
Then you can install in this environment `xeus-cpp` and its dependencies.

.. code-block:: bash

    mamba install xeus-cpp notebook -c conda-forge

    mkdir build && cd build
    cmake .. -D CMAKE_PREFIX_PATH=$CONDA_PREFIX 
    -D CMAKE_INSTALL_PREFIX=$CONDA_PREFIX -D CMAKE_INSTALL_LIBDIR=lib
    make && make install