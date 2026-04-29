#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import subprocess

on_rtd = os.environ.get('READTHEDOCS', None) == 'True'

if on_rtd:
    XEUS_CPP_ROOT = os.path.abspath('../..')
    command_emscripten = f'''
cd ../../;
curl -L https://micro.mamba.pm/api/micromamba/linux-64/latest | tar -xvj bin/micromamba;
export PATH="$PWD/bin:$PATH";
export MAMBA_EXE="$PWD/bin/micromamba";
export MAMBA_ROOT_PREFIX="/home/docs/checkouts/readthedocs.org/user_builds/xeus-cpp/conda/";
__mamba_setup="$("$MAMBA_EXE" shell hook --shell bash --root-prefix "$MAMBA_ROOT_PREFIX" 2> /dev/null)";
eval "$__mamba_setup";
alias micromamba="$MAMBA_EXE";
micromamba create -f environment-wasm-build.yml -y;
micromamba activate xeus-cpp-wasm-build;
micromamba create -f environment-wasm-host.yml \
--platform=emscripten-wasm32 \
-c https://prefix.dev/emscripten-forge-4x \
-c https://prefix.dev/conda-forge
mkdir -p build;
cd build;
export BUILD_PREFIX=$MAMBA_ROOT_PREFIX/xeus-cpp-wasm-build;
export PREFIX=$MAMBA_ROOT_PREFIX/xeus-cpp-wasm-host;
export SYSROOT_PATH=$BUILD_PREFIX/opt/emsdk/upstream/emscripten/cache/sysroot;
emcmake cmake -DCMAKE_BUILD_TYPE=Release \\
              -DCMAKE_PREFIX_PATH=$PREFIX \\
              -DCMAKE_INSTALL_PREFIX=$PREFIX \\
              -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ON \\
              -DSYSROOT_PATH=$SYSROOT_PATH \\
              {XEUS_CPP_ROOT};
emmake make -j $(nproc --all) install;
cd {XEUS_CPP_ROOT};
micromamba create -n xeus-lite-host jupyter_server jupyterlite-xeus -c conda-forge -y;
micromamba activate xeus-lite-host;
jupyter lite build --XeusAddon.prefix=$PREFIX \\
                   --XeusAddon.mounts="$PREFIX/share/xeus-cpp/tagfiles:/share/xeus-cpp/tagfiles" \
                   --XeusAddon.mounts="$PREFIX/etc/xeus-cpp/tags.d:/etc/xeus-cpp/tags.d" \
                   --contents notebooks/xeus-cpp-lite-demo.ipynb \\
                   --contents notebooks/tinyraytracer.ipynb \\
                   --contents notebooks/images/marie.png \\
                   --contents notebooks/audio/audio.wav \\
                   --output-dir $READTHEDOCS_OUTPUT/html/xeus-cpp;
'''
    subprocess.call('cd ..; doxygen', shell=True)
    subprocess.run(['bash', '-c', command_emscripten], check=True)
    

import sphinx_rtd_theme

html_theme = "sphinx_rtd_theme"

html_theme_path = [sphinx_rtd_theme.get_html_theme_path()]

def setup(app):
    app.add_css_file("main_stylesheet.css")

extensions = ['breathe', 'sphinx_rtd_theme']
breathe_projects = { 'xeus-cpp': '../xml' }
templates_path = ['_templates']
html_static_path = ['_static']
source_suffix = '.rst'
master_doc = 'index'
project = 'xeus-cpp'
copyright = '2017, Johan Mabille, Loic Gouarin and Sylvain Corlay'
author = 'Johan Mabille, Loic Gouarin and Sylvain Corlay'

exclude_patterns = []
highlight_language = 'c++'
pygments_style = 'sphinx'
todo_include_todos = False
htmlhelp_basename = 'xeus-cppdoc'
