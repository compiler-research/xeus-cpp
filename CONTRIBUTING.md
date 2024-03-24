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

First, you need to fork the project. Then setup your environment:

```bash
# create a new conda environment
conda env create -f environment-dev.yml
conda activate xeus-cpp

# download xeus-cpp from your GitHub fork
git clone https://github.com/<your-github-username>/xeus-cpp.git
```

You may also want to install a C++ compiler, and cmake from conda if they are not
available on your system.

## Building and installing xeus-cpp

```bash
# Create a directory for building
mkdir build && cd build
# Generate the makefile with cmake
cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_PREFIX_PATH=$CONDA_PREFIX -D CMAKE_INSTALL_PREFIX=$CONDA_PREFIX -D CMAKE_INSTALL_LIBDIR=lib ..
# Build and install
make install -j2
```

## Running the tests

To run Python tests, from the build directory, type

```bash
cd ../test
pytest . -vvv
```
