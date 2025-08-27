Juche
=====

Self-reliant build system. To use it, include `juche.c` in a build script. The
example provided in `build.c` shows how to make it automatically rebuild itself.
More comments in the source code.

Note: Uses POSIX functions

Building
========

Bootstrap the build system:
cc ./build.c -o ./build/juche

Run the build script:
./build/juche
