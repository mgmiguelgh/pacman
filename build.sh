#!/bin/sh
if [[ ${1^^} == "RELEASE" ]]; then
    compiler_flags="-O2"
else
    compiler_flags="-O0 -g"
fi

clang $compiler_flags -std=c99 -Wall src/linux/linux_pacman.c -D_GNU_SOURCE -lX11 -lGL -lm -o pacman
