Juche Build System
==================

Self-reliant build system. To use it, include `juche.c` in a build script. The
example provided in `build.c` shows how to make it automatically rebuild itself.

How to use
==========

Bootstrap build system:
cc ./build.c -o ./build/juche

Run it:
./build/juche | sh
