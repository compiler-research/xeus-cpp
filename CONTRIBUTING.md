# Contributing to xeus-cpp

Xeus and xeus-cpp are subprojects of Project Jupyter and subject to the
[Jupyter governance](https://github.com/jupyter/governance) and
[Code of conduct](https://github.com/jupyter/governance/blob/master/conduct/code_of_conduct.md).

## General Guidelines

For general documentation about contributing to Jupyter projects, see the
[Project Jupyter Contributor Documentation](https://docs.jupyter.org/en/latest/contributing/content-contributor.html).

## Community

The Xeus team organizes public video meetings. The schedule for future meetings and
minutes of past meetings can be found on our
[team compass](https://jupyter-xeus.github.io/).

## Setting up a development environment

First, you need to fork the project. After you have done this clone your forked repo. You can do this by 
executing the folowing

```bash
git clone https://github.com/<your-github-username>/xeus-cpp.git
```

To ensure that the installation works, it is preferable to install xeus-cpp in a fresh environment. It is
also needed to use a miniforge or miniconda installation because with the full anaconda you may have a 
conflict with the zeromq library which is already installed in the anaconda distribution. Once you have miniforge 
or miniconda installed cd into the xeus-cpp directory and set setup your environment:

```bash
cd xeus-cpp
micromamba create -f environment-dev.yml -y
micromamba activate xeus-cpp
```

You are now in a position to install xeus-cpp into this envirnoment. You can do this by executing

```bash
mkdir build
cd build
cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_PREFIX_PATH=$CONDA_PREFIX -D CMAKE_INSTALL_PREFIX=$CONDA_PREFIX -D CMAKE_INSTALL_LIBDIR=lib ..
make install
```

To check that everything is installed correctly you can run the c++ tests by executing the following

```bash
cd ./test
./test_xeus_cpp
```

and the python tests by executing

```bash
cd ../../test
pytest -sv test_xcpp_kernel.py
```

## Setting up a development environment (wasm instructions)

First, you need to fork the project. After you have done this clone your forked repo. You can do this by 
executing the folowing

```bash
git clone https://github.com/<your-github-username>/xeus-cpp.git
cd ./xeus-cpp
```

You'll now want to make sure you are using the same emsdk as the rest of our dependencies. This can be achieved by executing 
the following
```bash
micromamba create -f environment-wasm-build.yml -y
micromamba activate xeus-cpp-wasm-build
```

You are now in a position to build the xeus-cpp kernel. You build it by executing the following
```bash
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
```

To build Jupyter Lite with this kernel without creating a website you can execute the following
```bash
micromamba create -n xeus-lite-host jupyterlite-core -c conda-forge
micromamba activate xeus-lite-host
python -m pip install jupyterlite-xeus
jupyter lite build --XeusAddon.prefix=$PREFIX
```

Once the Jupyter Lite site has built you can test the website locally by executing
```bash
jupyter lite serve --XeusAddon.prefix=$PREFIX
```
