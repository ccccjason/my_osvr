#!/bin/sh -e
# For use with MSYS2 and MinGW64-i686 toolchain
CC=gcc ./configure.sh --with-dl=both --with-id-anchor --with-github-tags --with-fenced-code --with-urlencoded-anchor
make markdown
mv markdown.exe markdown.exe.save
make clean
mv markdown.exe.save markdown.exe
