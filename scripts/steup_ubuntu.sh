#!/usr/bin/env bash
set -e

sudo apt update

sudo apt install -y \
    build-essential \
    gdb \
    clangd \
    clang-format \
    cmake \
    ninja-build \
    make \
    git \
    bear \
    valgrind \
    strace \
    ltrace \
    net-tools \
    iproute2 \
    tcpdump \
    tree \
    curl \
    netcat-openbsd