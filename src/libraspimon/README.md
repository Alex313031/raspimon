# libraspimon

This is a static/shared library that contains the core of the hardware monitoring functions for raspimon.

It defaults to static, to build the shared `.so` version, we define `LIBRASPIMON_SHARED=1`. To do this:


```bash
make SHARED_LIBRASPIMON=1 # make

cmake SHARED_LIBRASPIMON=1 # cmake

shared_libraspimon = true # in args.gn, for GN/Ninja
```
