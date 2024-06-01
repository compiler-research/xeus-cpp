# ![xeus-cpp](docs/source/xeus-logo.svg)

[![Build Status](https://github.com/compiler-research/xeus-cpp/actions/workflows/main.yml/badge.svg)](https://github.com/compiler-research/xeus-cpp/actions/workflows/main.yml)
[![Documentation Status](http://readthedocs.org/projects/xeus-cpp/badge/?version=latest)](https://xeus-cppreadthedocs.io/en/latest/?badge=latest)
[![Binder](https://mybinder.org/badge_logo.svg)](https://mybinder.org/v2/gh/compiler-research/xeus-cpp/main?urlpath=/lab/tree/notebooks/xeus-cpp.ipynb)
[![codecov]( https://codecov.io/gh/compiler-research/xeus-cpp/branch/main/graph/badge.svg)](https://codecov.io/gh/compiler-research/xeus-cpp)

[![Conda-Forge](https://img.shields.io/conda/vn/conda-forge/xeus-cpp)](https://github.com/conda-forge/xeus-cpp-feedstock)
[![Anaconda-Server Badge](https://anaconda.org/conda-forge/xeus-cpp/badges/license.svg)](https://github.com/conda-forge/xeus-cpp-feedstock)
[![Conda Platforms](https://img.shields.io/conda/pn/conda-forge/xeus-cpp.svg)](https://anaconda.org/conda-forge/xeus-cpp)
[![Anaconda-Server Badge](https://anaconda.org/conda-forge/xeus-cpp/badges/downloads.svg)](https://github.com/conda-forge/xeus-cpp-feedstock)


`xeus-cpp` is a Jupyter kernel for cpp based on the native implementation of the
Jupyter protocol [xeus](https://github.com/jupyter-xeus/xeus).

## Installation within a mamba environment (non wasm build instructions)

To ensure that the installation works, it is preferable to install `xeus-cpp` in a
fresh environment. It is also needed to use a
[miniforge](https://github.com/conda-forge/miniforge#mambaforge) or
[miniconda](https://conda.io/miniconda.html) installation because with the full
[anaconda](https://www.anaconda.com/) you may have a conflict with the `zeromq` library
which is already installed in the anaconda distribution.

First clone the repository, and move into that directory
```bash
git clone --depth=1 https://github.com/compiler-research/xeus-cpp.git
cd ./xeus-cpp
```
The safest usage of xeus-cpp from source is to build and install it within a clean environment named `xeus-cpp`. You can create and activate this environment 
with mamba by executing the following
```bash
mamba create -n  "xeus-cpp"
source activate  "xeus-cpp"
```
We will now install the dependencies needed to compile xeux-cpp from source within this environment by executing the following
```bash
mamba install notebook cmake cxx-compiler xeus-zmq nlohmann_json=3.11.2 cppzmq xtl jupyterlab CppInterOp cpp-argparse<3.1 pugixml doctest -c conda-forge
```
Now you can compile the kernel from the source by executing (replace `$CONDA_PREFIX` with a custom installation prefix if need be)
```bash
mkdir build
cd build
cmake .. -D CMAKE_PREFIX_PATH=$CONDA_PREFIX -D CMAKE_INSTALL_PREFIX=$CONDA_PREFIX -D CMAKE_INSTALL_LIBDIR=lib
make install
```
To test the build you execute the following to test the C++ tests
```bash
cd test
./test_xeus_cpp
```
and
```bash
cd ../../test
pytest -sv test_xcpp_kernel.py
```
to perform the python tests.

## Installation within a mamba environment (wasm build instructions)

First clone the repository, and move into that directory
```bash
git clone --depth=1 https://github.com/compiler-research/xeus-cpp.git
cd ./xeus-cpp
```

Now you'll want to create a clean mamba environment containing the tools you'll need to do a wasm build. This can be done by executing 
the following
```bash
micromamba create -f environment-wasm-build.yml -y
micromamba activate xeus-cpp-wasm-build
```

You'll now want to make sure you're using emsdk version "3.1.45" and activate it. You can get this by executing the following
```bash
emsdk install 3.1.45
emsdk activate 3.1.45
source $CONDA_EMSDK_DIR/emsdk_env.sh
```

You are now in a position to build the xeus-cpp kernel. You build it by executing the following
```bash
micromamba create -f environment-wasm-host.yml --platform=emscripten-wasm32
mkdir build
pushd build
export EMPACK_PREFIX=$MAMBA_ROOT_PREFIX/envs/xeus-cpp-wasm-build
export PREFIX=$MAMBA_ROOT_PREFIX/envs/xeus-cpp-wasm-host 
export CMAKE_PREFIX_PATH=$PREFIX
export CMAKE_SYSTEM_PREFIX_PATH=$PREFIX

emcmake cmake \
        -DCMAKE_BUILD_TYPE=Release                        \
        -DCMAKE_PREFIX_PATH=$PREFIX                       \
        -DCMAKE_INSTALL_PREFIX=$PREFIX                    \
        -DXEUS_CPP_EMSCRIPTEN_WASM_BUILD=ON               \
        -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ON            \
        ..
EMCC_CFLAGS='-sERROR_ON_UNDEFINED_SYMBOLS=0' emmake make install
```

To build Jupyter Lite with this kernel without creating a website you can execute the following
```bash
micromamba create -n xeus-lite-host jupyterlite-core
micromamba activate xeus-lite-host
python -m pip install jupyterlite-xeus
jupyter lite build --XeusAddon.prefix=$PREFIX
```
Once the Jupyter Lite site has built you can test the website locally by executing
```bash
jupyter lite serve --XeusAddon.prefix=$PREFIX
```


## Trying it online

To try out xeus-cpp interactively in your web browser, just click on the binder link:

[![Binder](binder-logo.svg)](https://mybinder.org/v2/gh/compiler-research/xeus-cpp/main?urlpath=/lab/tree/notebooks/xeus-cpp.ipynb) 

## Documentation

To get started with using `xeus-cpp`, check out the full documentation

http://xeus-cpp.readthedocs.io

## Dependencies

`xeus-cpp` depends on

- [xeus-zmq](https://github.com/jupyter-xeus/xeus-zmq)
- [xtl](https://github.com/xtensor-stack/xtl)
- [nlohmann_json](https://github.com/nlohmann/json)
- [cppzmq](https://github.com/zeromq/cppzmq)
- [clang](https://github.com/llvm/llvm-project/)
- [argparse](https://github.com/p-ranav/argparse)

| `xeus-cpp` | `xeus-zmq`      | `xtl`           | `CppInterOp` | `clang`   | `pugixml` | `cppzmq` | `cpp-argparse`| `nlohmann_json` |
|------------|-----------------|-----------------|--------------|-----------|-----------|----------|---------------|-----------------|
|  main      |  >=1.0.2,<2.0.0 |  >=0.7.7,<0.8.0 | >=1.3.0      |           | ~1.8.1    | ~4.3.0   | <3.1          | >=3.11.2,<4.0   |
|  0.4.0     |  >=1.0.0,<2.0.0 |  >=0.7.7,<0.8.0 |              | >=16,<17  | ~1.8.1    | ~4.3.0   | ~2.9          | >=3.6.1,<4.0    |
|  0.3.0     |  >=1.0.0,<2.0.0 |  >=0.7.7,<0.8.0 |              | >=16,<17  | ~1.8.1    | ~4.3.0   | ~2.9          | >=3.6.1,<4.0    |
|  0.2.0     |  >=1.0.0,<2.0.0 |  >=0.7.7,<0.8.0 |              | >=16,<17  | ~1.8.1    | ~4.3.0   | ~2.9          | >=3.6.1,<4.0    |
|  0.1.0     |  >=1.0.0,<2.0.0 |  >=0.7.0,<0.8.0 |              | >=16,<17  | ~1.8.1    | ~4.3.0   | ~2.9          | >=3.6.1,<4.0    |

## Contributing

See [CONTRIBUTING.md](./CONTRIBUTING.md) to know how to contribute and set up a
development environment.

## License

This software is licensed under the `BSD 3-Clause License`. See the [LICENSE](LICENSE)
file for details.
