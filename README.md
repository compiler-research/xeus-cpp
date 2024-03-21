# ![xeus-cpp](docs/source/xeus-logo.svg)

[![Build Status](https://github.com/compiler-research/xeus-cpp/actions/workflows/main.yml/badge.svg)](https://github.com/compiler-research/xeus-cpp/actions/workflows/main.yml)
[![Documentation Status](http://readthedocs.org/projects/xeus-cpp/badge/?version=latest)](https://xeus-cppreadthedocs.io/en/latest/?badge=latest)
[![Binder](https://mybinder.org/badge_logo.svg)](https://mybinder.org/v2/gh/compiler-research/xeus-cpp/main?urlpath=/lab/tree/notebooks/xeus-cpp.ipynb)
[![codecov]( https://codecov.io/gh/compiler-research/xeus-cpp/branch/main/graph/badge.svg)](https://codecov.io/gh/compiler-research/xeus-cpp)

`xeus-cpp` is a Jupyter kernel for cpp based on the native implementation of the
Jupyter protocol [xeus](https://github.com/jupyter-xeus/xeus).

## Installation within a mamba envirnoment

xeus-cpp has not been packaged for the mamba (or conda) package manager.

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
The safest usage of xeus-cpp is to build and install it within a clean environment named `xeus-cpp`. You can create and activate this environment 
with mamba by executing the following
```bash
mamba create -n  "xeus-cpp"
source activate  "xeus-cpp"
```
We will now install the dependencies needed to compile xeux-cpp from source within this environment by executing the following
```bash
mamba install notebook cmake cxx-compiler xeus-zmq nlohmann_json=3.11.2 cppzmq xtl jupyterlab clangdev=16 cpp-argparse pugixml doctest -c conda-forge
```
Now you can compile the kernel from the source by executing (replace `$CONDA_PREFIX` with a custom installation prefix if need be)
```bash
mkdir build && cd build
cmake .. -D CMAKE_PREFIX_PATH=$CONDA_PREFIX -D CMAKE_INSTALL_PREFIX=$CONDA_PREFIX -D CMAKE_INSTALL_LIBDIR=lib
make && make install
```

<!-- ## Trying it online

To try out xeus-cpp interactively in your web browser, just click on the binder link:
(Once Conda Package is Ready)

[![Binder](binder-logo.svg)](https://mybinder.org/v2/gh/compiler-research/xeus-cpp/main?urlpath=/lab/tree/notebooks/xeus-cpp.ipynb) -->

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

| `xeus-cpp` | `xeus-zmq`      | `xtl`           | `clang`   | `pugixml` | `cppzmq` | `cpp-argparse`| `nlohmann_json` | `dirent` (windows only) |
|------------|-----------------|-----------------|-----------|-----------|----------|---------------|-----------------|-------------------------|
|  main      |  >=1.0.0,<2.0.0 |  >=0.7.7,<0.8.0 | >=16,<17  | ~1.8.1    | ~4.3.0   | ~2.9          | >=3.6.1,<4.0    | >=2.3.2,<3              |
|  0.3.0     |  >=1.0.0,<2.0.0 |  >=0.7.7,<0.8.0 | >=16,<17  | ~1.8.1    | ~4.3.0   | ~2.9          | >=3.6.1,<4.0    | >=2.3.2,<3              |
|  0.2.0     |  >=1.0.0,<2.0.0 |  >=0.7.7,<0.8.0 | >=16,<17  | ~1.8.1    | ~4.3.0   | ~2.9          | >=3.6.1,<4.0    | >=2.3.2,<3              |
|  0.1.0     |  >=1.0.0,<2.0.0 |  >=0.7.0,<0.8.0 | >=16,<17  | ~1.8.1    | ~4.3.0   | ~2.9          | >=3.6.1,<4.0    | >=2.3.2,<3              |
|  0.0.1     |  >=1.0.0,<2.0.0 |  >=0.7.0,<0.8.0 | >=16,<17  | ~1.8.1    | ~4.3.0   | ~2.9          | >=3.6.1,<4.0    | >=2.3.2,<3              |

## Contributing

See [CONTRIBUTING.md](./CONTRIBUTING.md) to know how to contribute and set up a
development environment.

## License

This software is licensed under the `BSD 3-Clause License`. See the [LICENSE](LICENSE)
file for details.
