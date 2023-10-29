#!/bin/bash
docker container run --rm -i hadolint/hadolint hadolint - < Dockerfile

jupyter-repo2docker \
    --no-run \
    --user-name=jovyan \
    --image-name xeus-cpp \
    .

#docker run --gpus all --publish 8888:8888 --name xeus-cpp-c -i -t xeus-cpp "start-notebook.sh"
docker run --rm --runtime=nvidia --gpus all --publish 9999:9999 --name xeus-cpp-c -i -t xeus-cpp "start-notebook.sh"

#    --editable \
#    --ref InterOpIntegration \
#    https://github.com/alexander-penev/xeus-cpp.git \
