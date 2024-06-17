Installation And Usage
--------------------

Installation from source
========================

To ensure that the installation works, it is preferable to install `xeus-cpp` in a
fresh environment. It is also needed to use a miniforge or miniconda installation 
because with the full Anaconda installation you may have a conflict with the `zeromq` 
library which is already installed in the anaconda distribution. First clone the 
repository, and move into that directory

.. code-block:: bash

    git clone --depth=1 https://github.com/compiler-research/xeus-cpp.git
    cd ./xeus-cpp

The safest usage of xeus-cpp from source is to build and install it within a 
clean environment named `xeus-cpp`. You can create and activate 
this environment with mamba by executing the following

.. code-block:: bash

    mamba create -n  "xeus-cpp"
    source activate  "xeus-cpp"

We will now install the dependencies needed to compile xeux-cpp from source within 
this environment by executing the following

.. code-block:: bash

    mamba install notebook cmake cxx-compiler xeus-zmq nlohmann_json=3.11.3
    jupyterlab CppInterOp cpp-argparse<3.1 pugixml doctest -c conda-forge

Now you can compile the kernel from the source by executing (replace `$CONDA_PREFIX` 
with a custom installation prefix if need be)

.. code-block:: bash

    mkdir build && cd build
    cmake .. -D CMAKE_PREFIX_PATH=$CONDA_PREFIX -D CMAKE_INSTALL_PREFIX=$CONDA_PREFIX 
    -D CMAKE_INSTALL_LIBDIR=lib
    make && make install

Installing from conda-forge
===========================

If you have conda installed then you can install xeus-cpp using the follwing command

.. code-block:: bash

    conda install conda-forge::xeus-cpp

Xeus-cpp is available for Linux, MacOS and Windows.