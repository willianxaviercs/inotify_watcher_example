#!/bin/bash

set -e

OUTPUT_DIR=./build
SRC_DIR=./src
BIN_NAME=inotify_example

mkdir -p ./build

gcc                                       \
    -Wno-unused-parameter                 \
    -Wall -Wextra -Wconversion -Werror    \
        -fsanitize=address                \
        -fno-omit-frame-pointer           \
        -g                                \
        -o  ${OUTPUT_DIR}/${BIN_NAME} ${SRC_DIR}/main.c

