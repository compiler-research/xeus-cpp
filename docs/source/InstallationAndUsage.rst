Installation And Usage
--------------------

Installation from source (non wasm build instructions)
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

Installation within a mamba environment (wasm build instructions)
========================

These instructions will assume you have cmake installed on your system. First clone the repository, and move into that directory
.. code-block:: bash
    git clone --depth=1 https://github.com/compiler-research/xeus-cpp.git
    cd ./xeus-cpp


You'll now want to make sure you're using emsdk version "3.1.45" and activate it. You can get this by executing the following

.. code-block:: bash
    cd $HOME
    git clone https://github.com/emscripten-core/emsdk.git
    cd emsdk
    ./emsdk install 3.1.45
    ./emsdk activate 3.1.45
    source $HOME/emsdk/emsdk_env.sh


You are now in a position to build the xeus-cpp kernel. You build it by executing the following

.. code-block:: bash
    micromamba create -f environment-wasm-host.yml --platform=emscripten-wasm32
    mkdir build
    pushd build
    export PREFIX=$MAMBA_ROOT_PREFIX/envs/xeus-cpp-wasm-host 
    export SYSROOT_PATH=$HOME/emsdk/upstream/emscripten/cache/sysroot
    emcmake cmake \
            -DCMAKE_BUILD_TYPE=Release                        \
            -DCMAKE_INSTALL_PREFIX=$PREFIX                    \
            -DXEUS_CPP_EMSCRIPTEN_WASM_BUILD=ON               \
            -DCMAKE_FIND_ROOT_PATH=$PREFIX                    \
            -DSYSROOT_PATH=$SYSROOT_PATH                      \
            ..
    emmake make install


To build Jupyter Lite with this kernel without creating a website you can execute the following

.. code-block:: bash
    micromamba create -n xeus-lite-host jupyterlite-core
    micromamba activate xeus-lite-host
    python -m pip install jupyterlite-xeus
    jupyter lite build --XeusAddon.prefix=$PREFIX

We now need to shift necessary files like `xcpp.data` which contains the binary representation of the file(s)
we want to include in our application. As of now this would contain all important files like Standard Headers,
Libraries etc coming out of emscripten's sysroot. Assuming we are still inside build we should do the following

.. code-block:: bash
    cp $PREFIX/bin/xcpp.data _output/extensions/@jupyterlite/xeus/static
    cp $PREFIX/lib/libclangCppInterOp.so _output/extensions/@jupyterlite/xeus/static

Once the Jupyter Lite site has built you can test the website locally by executing

.. code-block:: bash
    jupyter lite serve --XeusAddon.prefix=$PREFIX

Installing from conda-forge
===========================

If you have conda installed then you can install xeus-cpp using the follwing command

.. code-block:: bash

    conda install conda-forge::xeus-cpp

Xeus-cpp is available for Linux, MacOS and Windows.
