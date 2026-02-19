# ![xeus-cpp](docs/source/xeus-logo.svg)
<div align="center">
        
[![Build Status](https://github.com/compiler-research/xeus-cpp/actions/workflows/main.yml/badge.svg)](https://github.com/compiler-research/xeus-cpp/actions/workflows/main.yml)
[![Documentation Status](http://readthedocs.org/projects/xeus-cpp/badge/?version=latest)](https://xeus-cpp.readthedocs.io/en/latest/?badge=latest)
[![Binder](https://mybinder.org/badge_logo.svg)](https://mybinder.org/v2/gh/compiler-research/xeus-cpp/main?urlpath=/lab/tree/notebooks/xeus-cpp.ipynb)
[![codecov]( https://codecov.io/gh/compiler-research/xeus-cpp/branch/main/graph/badge.svg)](https://codecov.io/gh/compiler-research/xeus-cpp)

[![Conda-Forge](https://img.shields.io/conda/vn/conda-forge/xeus-cpp)](https://github.com/conda-forge/xeus-cpp-feedstock)
[![Anaconda-Server Badge](https://anaconda.org/conda-forge/xeus-cpp/badges/license.svg)](https://github.com/conda-forge/xeus-cpp-feedstock)
[![Conda Platforms](https://img.shields.io/conda/pn/conda-forge/xeus-cpp.svg)](https://anaconda.org/conda-forge/xeus-cpp)
[![Anaconda-Server Badge](https://anaconda.org/conda-forge/xeus-cpp/badges/downloads.svg)](https://github.com/conda-forge/xeus-cpp-feedstock)
</div>

`xeus-cpp` is a Jupyter kernel for cpp based on the native implementation of the
Jupyter protocol [xeus](https://github.com/jupyter-xeus/xeus).

Join our discord for discussions and collaboration.

<a target="_blank" href="https://discord.gg/Vkv3ne4zVK"><img src="discord.svg" alt="Discord" /></a>

To try a Jupyter Lite demo based on the main branch

<div align="center">
        
[![lite-badge](https://jupyterlite.rtfd.io/en/latest/_static/badge.svg)](https://compiler-research.github.io/xeus-cpp/lab/index.html)

</div>

To try a Jupyter Lite demo based on the latest release of xeus-cpp, which shows more of its capabilitities, including working with third party libraries,
click on the following

<div align="center">

[![lite-badge](https://jupyterlite.rtfd.io/en/latest/_static/badge.svg)](https://compiler-research.org/xeus-cpp-wasm/lab/index.html)

</div>

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
micromamba create -f environment-dev.yml
micromamba activate xeus-cpp
micromamba install jupyterlab -c conda-forge
```
Now you can compile the kernel from the source and run the C++ tests by executing (replace `$CONDA_PREFIX` with a custom installation prefix if need be)
```bash
mkdir build
cd build
cmake .. -D CMAKE_PREFIX_PATH=$CONDA_PREFIX -D CMAKE_INSTALL_PREFIX=$CONDA_PREFIX -D CMAKE_INSTALL_LIBDIR=lib
make check-xeus-cpp
make install
```
and
```bash
cd ../test
pytest -sv test_xcpp_kernel.py
```
to perform the python tests.

## Installation within a mamba environment (wasm build instructions)

These instructions will assume you have cmake installed on your system. First clone the repository, and move into that directory
```bash
git clone --depth=1 https://github.com/compiler-research/xeus-cpp.git
cd ./xeus-cpp
```

You'll now want to make sure you are using the same emsdk as the rest of our dependencies. This can be achieved by executing 
the following
```bash
micromamba create -f environment-wasm-build.yml -y
micromamba activate xeus-cpp-wasm-build
```

You are now in a position to build the xeus-cpp kernel. You build and test it in node by executing the following. Once the test pass, run the install command.
```bash
micromamba create -f environment-wasm-host.yml \
--platform=emscripten-wasm32 \
-c https://prefix.dev/emscripten-forge-4x \
-c https://prefix.dev/conda-forge

mkdir build
cd build
export BUILD_PREFIX=$MAMBA_ROOT_PREFIX/envs/xeus-cpp-wasm-build
export PREFIX=$MAMBA_ROOT_PREFIX/envs/xeus-cpp-wasm-host
export SYSROOT_PATH=$BUILD_PREFIX/opt/emsdk/upstream/emscripten/cache/sysroot

emcmake cmake \
        -DCMAKE_BUILD_TYPE=Release                        \
        -DCMAKE_INSTALL_PREFIX=$PREFIX                    \
        -DCMAKE_FIND_ROOT_PATH=$PREFIX                    \
        -DSYSROOT_PATH=$SYSROOT_PATH                      \
        ..

emmake make check-xeus-cpp
emmake make install
```

It is possible to run the Emscripten tests in a headless browser. We will run our tests in a fresh installed browser. Installing the browsers, and running the tests within the installed browsers will be platform dependent. To do this for Chrome and Firefox on MacOS execute the following from the build folder

```bash
cd  test
wget "https://download.mozilla.org/?product=firefox-latest&os=osx&lang=en-US" -O Firefox-latest.dmg
hdiutil attach Firefox-latest.dmg
cp -r /Volumes/Firefox/Firefox.app $PWD
hdiutil detach /Volumes/Firefox
cd ./Firefox.app/Contents/MacOS/
export PATH="$PWD:$PATH"
cd -

wget https://dl.google.com/chrome/mac/stable/accept_tos%3Dhttps%253A%252F%252Fwww.google.com%252Fintl%252Fen_ph%252Fchrome%252Fterms%252F%26_and_accept_tos%3Dhttps%253A%252F%252Fpolicies.google.com%252Fterms/googlechrome.pkg
pkgutil --expand-full googlechrome.pkg google-chrome
cd ./google-chrome/GoogleChrome.pkg/Payload/Google\ Chrome.app/Contents/MacOS/
export PATH="$PWD:$PATH"
cd -

echo "Running test_xeus_cpp in Firefox"
python $BUILD_PREFIX/bin/emrun.py --browser="firefox" --kill_exit --timeout 60 --browser-args="--headless"  test_xeus_cpp.html
echo "Running test_xeus_cpp in Google Chrome"
python python $BUILD_PREFIX/bin/emrun.py --browser="Google Chrome" --kill_exit --timeout 60 --browser-args="--headless  --no-sandbox"  test_xeus_cpp.html
```

To run tests in Safari you can make use of safaridriver. How to enable this will depend on
your MacOS operating system, and is best to consult [safaridriver](https://developer.apple.com/documentation/webkit/testing-with-webdriver-in-safari). You will also need to install the Selenium
python package. This only needs to be enable once, and then you can execute the following to run the tests in Safari

```bash
echo "Running test_xeus_cpp in Safari"
python $BUILD_PREFIX/bin/emrun.py --no_browser --kill_exit --timeout 60 --browser-args="--headless --no-sandbox"  test_xeus_cpp.html &
python ../../scripts/browser_tests_safari.py test_xeus_cpp.html
```

To run the browser tests on Ubuntu x86 execute the following from the build folder

```bash
cd test
wget https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb
dpkg-deb -x google-chrome-stable_current_amd64.deb $PWD/chrome
cd ./chrome/opt/google/chrome/
export PATH="$PWD:$PATH"
cd -

wget https://ftp.mozilla.org/pub/firefox/releases/138.0.1/linux-x86_64/en-GB/firefox-138.0.1.tar.xz
tar -xJf firefox-138.0.1.tar.xz
cd ./firefox
export PATH="$PWD:$PATH"
cd -

echo "Running test_xeus_cpp in Firefox"
python $BUILD_PREFIX/bin/emrun.py --browser="firefox" --kill_exit --timeout 60 --browser-args="--headless"  test_xeus_cpp.html
echo "Running test_xeus_cpp in Google Chrome"
python $BUILD_PREFIX/bin/emrun.py --browser="google-chrome" --kill_exit --timeout 60 --browser-args="--headless --no-sandbox"  test_xeus_cpp.html
```

To build and test Jupyter Lite with this kernel locally you can execute the following
```bash
micromamba create -n xeus-lite-host jupyter_server jupyterlite-xeus -c conda-forge
micromamba activate xeus-lite-host
jupyter lite serve --XeusAddon.prefix=$PREFIX \            
                   --XeusAddon.mounts="$PREFIX/share/xeus-cpp/tagfiles:/share/xeus-cpp/tagfiles" \
                   --XeusAddon.mounts="$PREFIX/etc/xeus-cpp/tags.d:/etc/xeus-cpp/tags.d" \
                   --contents README.md \
                   --contents notebooks/xeus-cpp-lite-demo.ipynb \
                   --contents notebooks/tinyraytracer.ipynb \
                   --contents notebooks/images/marie.png \
                   --contents notebooks/audio/audio.wav
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
- [nlohmann_json](https://github.com/nlohmann/json)
- [argparse](https://github.com/p-ranav/argparse)
- [CppInterOp](https://github.com/compiler-research/CppInterOp)
- [pugixml](https://github.com/zeux/pugixml)

| `xeus-cpp` | `xeus-zmq`      | `CppInterOp` | `pugixml` | `cpp-argparse`| `nlohmann_json` |
|------------|-----------------|--------------|-----------|---------------|-----------------|
|  main      |  3.1.0 | 1.8.0      | 1.15    | 3.2    | 3.12.0   |

## Contributing

See [CONTRIBUTING.md](./CONTRIBUTING.md) to know how to contribute and set up a
development environment.

## License

This software is licensed under the `BSD 3-Clause License`. See the [LICENSE](LICENSE)
file for details.
