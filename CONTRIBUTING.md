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
executing the following

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

You are now in a position to install xeus-cpp into this environment. You can do this by executing

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
executing the following

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
cd build
export BUILD_TOOLS_PREFIX=$MAMBA_ROOT_PREFIX/envs/xeus-cpp-wasm-build
export PREFIX=$MAMBA_ROOT_PREFIX/envs/xeus-cpp-wasm-host
export SYSROOT_PATH=$BUILD_TOOLS_PREFIX/opt/emsdk/upstream/emscripten/cache/sysroot

emcmake cmake \
        -DCMAKE_BUILD_TYPE=Release                        \
        -DCMAKE_INSTALL_PREFIX=$PREFIX                    \
        -DXEUS_CPP_EMSCRIPTEN_WASM_BUILD=ON               \
        -DCMAKE_FIND_ROOT_PATH=$PREFIX                    \
        -DSYSROOT_PATH=$SYSROOT_PATH                      \
        ..
emmake make install
```

To test the lite build you can execute the following to run the C++ tests built against emscripten in node

```bash
cd test
node test_xeus_cpp.js
```

It is possible to run the Emscripten tests in a headless browser. We will run our tests in a fresh installed browser. Installing the browsers, and running the tests within the installed browsers will be platform dependent. To do this on MacOS execute the following

```bash
wget "https://download.mozilla.org/?product=firefox-latest&os=osx&lang=en-US" -O Firefox-latest.dmg
hdiutil attach Firefox-latest.dmg
cp -r /Volumes/Firefox/Firefox.app $PWD
hdiutil detach /Volumes/Firefox
cd ./Firefox.app/Contents/MacOS/
export PATH="$PWD:$PATH"
cd –

wget https://dl.google.com/chrome/mac/stable/accept_tos%3Dhttps%253A%252F%252Fwww.google.com%252Fintl%252Fen_ph%252Fchrome%252Fterms%252F%26_and_accept_tos%3Dhttps%253A%252F%252Fpolicies.google.com%252Fterms/googlechrome.pkg
pkgutil --expand-full googlechrome.pkg google-chrome
cd ./google-chrome/GoogleChrome.pkg/Payload/Google\ Chrome.app/Contents/MacOS/
export PATH="$PWD:$PATH"
cd –

echo "Running test_xeus_cpp in Firefox"
python $BUILD_PREFIX/bin/emrun.py --browser="firefox" --kill_exit --browser-args="--headless"  test_xeus_cpp.html
echo "Running test_xeus_cpp in Google Chrome"
python $BUILD_PREFIX/bin/emrun.py --browser="Google Chrome" --kill_exit --browser-args="--headless  --no-sandbox"  test_xeus_cpp.html
```

To do this on Ubuntu x86 execute the following

```bash
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

To build Jupyter Lite with this kernel without creating a website you can execute the following
```bash
micromamba create -n xeus-lite-host jupyterlite-core=0.6 jupyter_server jupyterlite-xeus -c conda-forge
micromamba activate xeus-lite-host
jupyter lite build --XeusAddon.prefix=$PREFIX
```

Once the Jupyter Lite site has built you can test the website locally by executing
```bash
jupyter lite serve --XeusAddon.prefix=$PREFIX
```
