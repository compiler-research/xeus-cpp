# Copyright (c) Jupyter Development Team.
# Distributed under the terms of the Modified BSD License.

# https://hub.docker.com/r/jupyter/base-notebook/tags
ARG BASE_CONTAINER=jupyter/base-notebook
ARG BASE_TAG=latest
ARG BUILD_TYPE=Debug


FROM $BASE_CONTAINER:$BASE_TAG

LABEL maintainer="Xeus-cpp Project"

SHELL ["/bin/bash", "--login", "-o", "pipefail", "-c"]

USER root

ENV TAG="$BASE_TAG"

ENV LC_ALL=en_US.UTF-8 \
    LANG=en_US.UTF-8 \
    LANGUAGE=en_US.UTF-8

# Install all OS dependencies for notebook server that starts but lacks all
# features (e.g., download as all possible file formats)
RUN \
    set -x && \
    apt-get update --yes && \
    apt-get install --yes --no-install-recommends pciutils && \
    export _CUDA_="$(lspci -nn | grep '\[03' | grep NVIDIA)" && \
    apt-get install --yes --no-install-recommends \
      #fonts-liberation, pandoc, run-one are inherited from base-notebook container image
    # Other "our" apt installs
      unzip \
      curl \
      jq \
      ###libomp-dev \
    # Other "our" apt installs (development and testing)
      build-essential \
      git \
      nano-tiny \
      less \
      gdb valgrind \
      emacs \
    # CUDA
      #cuda \
      $([ -n "$_CUDA_" ] && echo nvidia-cuda-toolkit) \
    && \
    apt-get clean && rm -rf /var/lib/apt/lists/* && \
    echo "en_US.UTF-8 UTF-8" > /etc/locale.gen && \
    locale-gen

ENV LC_ALL=en_US.UTF-8 \
    LANG=en_US.UTF-8 \
    LANGUAGE=en_US.UTF-8

# Create alternative for nano -> nano-tiny
#RUN update-alternatives --install /usr/bin/nano nano /bin/nano-tiny 10

USER ${NB_UID}

# Copy git repository to home directory of container
COPY --chown=${NB_UID}:${NB_GID} . "${HOME}"/

EXPOSE 9999
ENV JUPYTER_PORT=9999

# Configure container startup
CMD ["start-notebook.sh", "--debug", "&>/home/jovyan/log.txt"]

USER root

# Fix start-notebook.sh
RUN sed -i '2 i source /home/jovyan/.conda.init && conda activate .venv' /usr/local/bin/start-notebook.sh

### Make /home/runner directory and fix permisions
##RUN mkdir /home/runner && fix-permissions /home/runner

# Switch back to jovyan to avoid accidental container runs as root
USER ${NB_UID}

WORKDIR "${HOME}"

ENV NB_PYTHON_PREFIX=${CONDA_DIR} \
    KERNEL_PYTHON_PREFIX=${CONDA_DIR} \
    VENV=${CONDA_DIR}/envs/.venv \
    # CUDA
    NVIDIA_VISIBLE_DEVICES=all \
    NVIDIA_DRIVER_CAPABILITIES=compute,utility \
    NVIDIA_REQUIRE_CUDA="cuda>=12.1.1 driver>=530" \
    #
    PATH=/opt/conda/envs/.venv/bin:/opt/conda/bin:/opt/conda/envs/.venv/bin:/opt/conda/condabin:/opt/conda/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
    LD_LIBRARY_PATH=/home/jovyan/cppyy-backend/python/cppyy_backend/lib:/opt/conda/envs/.venv/lib:/opt/conda/lib:/home/jovyan/cppyy-backend/python/cppyy_backend/lib:/opt/conda/envs/.venv/lib \
    PYTHONPATH=/home/jovyan/CPyCppyy/build:/home/jovyan/cppyy-backend/python:/home/jovyan \
    CPLUS_INCLUDE_PATH=/opt/conda/envs/.venv/include:\
/opt/conda/envs/.venv/include/python3.10:\
/home/jovyan/clad/include:\
/home/jovyan/CPyCppyy/include:\
/home/jovyan/cppyy-backend/python/cppyy_backend/include:\
/opt/conda/envs/.venv/include/llvm:\
/opt/conda/envs/.venv/include/clang:\
/opt/conda/include:\
/home/jovyan/clad/include:\
/home/jovyan/CppInterOp/include:\
/opt/conda/include:\
#
/opt/conda/envs/.venv/lib/gcc/x86_64-conda-linux-gnu/12.3.0/include:\
/opt/conda/envs/.venv/lib/gcc/x86_64-conda-linux-gnu/12.3.0/include-fixed:\
/opt/conda/envs/.venv/lib/gcc/x86_64-conda-linux-gnu/12.3.0/../../../../x86_64-conda-linux-gnu/include:\
/opt/conda/envs/.venv/x86_64-conda-linux-gnu/include/c++/12.3.0:\
/opt/conda/envs/.venv/x86_64-conda-linux-gnu/include/c++/12.3.0/x86_64-conda-linux-gnu:\
/opt/conda/envs/.venv/x86_64-conda-linux-gnu/include/c++/12.3.0/backward:\
/opt/conda/envs/.venv/x86_64-conda-linux-gnu/sysroot/usr/include:\
#
/usr/include

# VENV

# Jupyter Notebook, Lab, and Hub are installed in base image
# ReGenerate a notebook server config
# Cleanup temporary files
# Correct permissions
# Do all this in a single RUN command to avoid duplicating all of the
# files across image layers when the permissions change
#RUN mamba update --all --quiet --yes -c conda-forge && \
RUN \
    set -x && \
    # setup virtual environment
    mamba create -y -n .venv python=3.10.6 && \
    #
    #echo "echo \"@ @ @  PROFILE @ @ @ \"" >> ~/.profile && \
    #echo "echo \"@ @ @  BASHRC @ @ @ \"" >> /home/jovyan/.bashrc && \
    mv /home/jovyan/.bashrc /home/jovyan/.bashrc.tmp && \
    touch /home/jovyan/.bashrc && \
    conda init bash && \
    mv /home/jovyan/.bashrc /home/jovyan/.conda.init && \
    mv /home/jovyan/.bashrc.tmp /home/jovyan/.bashrc && \
    conda init bash && \
    echo "source /home/jovyan/.conda.init && conda activate .venv" >> /home/jovyan/.bashrc && \
    #
    source /home/jovyan/.conda.init && \
    conda activate .venv && \
    fix-permissions "${CONDA_DIR}" && \
    #
    mamba install --quiet --yes -c conda-forge \
      # notebook, jpyterhub, jupyterlab are inherited from base-notebook container image
    # Other "our" conda installs
      #
    # Build dependencies
      make \
      cmake \
      cxx-compiler \
    # Host dependencies
      'xeus-zmq>=1.0.2,<2.0' \
      nlohmann_json \
      cppzmq \
      xtl \
      'clangdev>=17' \
      'llvm-openmp' \
      pugixml \
      cpp-argparse \
      zlib \
    #
      ipykernel \
    # Test dependencies
      pytest \
      'jupyter_kernel_test>=0.4.3' \
      nbval \
      pytest-rerunfailures \
      && \
    hash -r && \
    pip install ipython && \
    #rm /home/jovyan/.jupyter/jupyter_notebook_config.py && \
    jupyter notebook --generate-config -y && \
    mamba clean --all -f -y && \
    npm cache clean --force && \
    jupyter lab clean && \
    rm -rf "/home/${NB_USER}/.cache/yarn" && \
    fix-permissions "${CONDA_DIR}" && \
    fix-permissions "/home/${NB_USER}"

### Post Build
RUN \
    set -x && \
    source /home/jovyan/.conda.init && \
    conda activate .venv && \
    source /home/jovyan/.conda.init && \
    conda activate .venv && \
    #
    export PATH_TO_LLVM_BUILD=${VENV} && \
    ###export PATH=${VENV}/bin:${CONDA_DIR}/bin:$PATH_TO_LLVM_BUILD/bin:$PATH && \
    ##export PATH=${VENV}/bin:${CONDA_DIR}/bin:$PATH && \
    echo "export EDITOR=emacs" >> ~/.profile && \
    #
    # Build CppInterOp
    #
    #sys_incs=$(LC_ALL=C c++ -xc++ -E -v /dev/null 2>&1 | LC_ALL=C sed -ne '/starts here/,/End of/p' | LC_ALL=C sed '/^ /!d' | cut -c2- | tr '\n' ':') && \
    git clone https://github.com/compiler-research/CppInterOp.git && \
    export CB_PYTHON_DIR="$PWD/cppyy-backend/python" && \
    export CPPINTEROP_DIR="$CB_PYTHON_DIR/cppyy_backend" && \
    cd CppInterOp && \
    mkdir build && \
    cd build && \
    export CPPINTEROP_BUILD_DIR=$PWD && \
    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DUSE_CLING=OFF -DUSE_REPL=ON -DLLVM_DIR=$PATH_TO_LLVM_BUILD -DLLVM_CONFIG_EXTRA_PATH_HINTS=${PATH_TO_LLVM_BUILD}/lib -DLLVM_USE_LINKER=gold -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=$CPPINTEROP_DIR .. && \
    cmake --build . --parallel $(nproc --all) && \
    #make install -j$(nproc --all)
    cd ../.. && \
    #
    # Build and Install cppyy-backend
    #
    git clone https://github.com/compiler-research/cppyy-backend.git && \
    cd cppyy-backend && \
    mkdir -p $CPPINTEROP_DIR/lib build && cd build && \
    # Install CppInterOp
    (cd $CPPINTEROP_BUILD_DIR && cmake --build . --target install --parallel $(nproc --all)) && \
    # Build and Install cppyy-backend
    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCppInterOp_DIR=$CPPINTEROP_DIR .. && \
    cmake --build . --parallel $(nproc --all) && \
    cp libcppyy-backend.so $CPPINTEROP_DIR/lib/ && \
    cd ../.. && \
    #
    # Build and Install CPyCppyy
    #
    # Install CPyCppyy
    git clone https://github.com/compiler-research/CPyCppyy.git && \
    cd CPyCppyy && \
    mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE .. && \
    cmake --build . --parallel $(nproc --all) && \
    export CPYCPPYY_DIR=$PWD && \
    cd ../.. && \
    #
    # Build and Install cppyy
    #
    # Install cppyy
    git clone https://github.com/compiler-research/cppyy.git && \
    cd cppyy && \
    python -m pip install --upgrade . --no-deps && \
    cd .. && \
    # Run cppyy
    #TODO: Fix cppyy path (/home/jovyan) to path to installed module
    python -c "import cppyy" && \
    #
    # Build and Install xeus-cpp
    #
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_PREFIX_PATH=$KERNEL_PYTHON_PREFIX -DCMAKE_INSTALL_PREFIX=$KERNEL_PYTHON_PREFIX -DCMAKE_INSTALL_LIBDIR=lib -DCPPINTEROP_DIR=$CPPINTEROP_BUILD_DIR .. && \
    make install -j$(nproc --all) && \
    cd .. && \
    #
    # Build and Install Clad
    #
    git clone --depth=1 https://github.com/vgvassilev/clad.git && \
    cd clad && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE .. -DClang_DIR=${PATH_TO_LLVM_BUILD}/lib/cmake/clang/ -DLLVM_DIR=${PATH_TO_LLVM_BUILD}/lib/cmake/llvm/ -DCMAKE_INSTALL_PREFIX=${CONDA_DIR} -DLLVM_EXTERNAL_LIT="$(which lit)" && \
    #make -j$(nproc --all) && \
    make && \
    make install && \
    #
    # xtensor
    #
    git clone https://github.com/xtensor-stack/xtensor.git && \
    cd xtensor && \
    mkdir build && cd build && \
    cmake -DCMAKE_INSTALL_PREFIX=$KERNEL_PYTHON_PREFIX .. && \
    make install && \
    #
    # Fixes and patches
    #
    # Web password and token set to ""
    echo "c.NotebookApp.token = ''" >> /home/jovyan/.jupyter/jupyter_notebook_config.py && \
    echo "c.NotebookApp.password = ''" >> /home/jovyan/.jupyter/jupyter_notebook_config.py && \
    # Patch /opt/conda/share/jupyter/kernels/python3/kernel.json to use .venv
    k="/opt/conda/share/jupyter/kernels/python3/kernel.json" && \
    jq ".argv[0] = \"${VENV}/bin/python\"" $k > $k.$$.tmp && mv $k.$$.tmp $k
