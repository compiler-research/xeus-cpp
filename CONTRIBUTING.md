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
micromamba create -n xeus-cpp environment-dev.yml
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