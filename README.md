# raspimon

A hardware monitor library and accompanying console/GUI utils for Linux on the Raspberry Pi 2/3/4/5.  
It can display clock speeds, temperatures, voltages, and more at a glance, refreshing periodically.

## About
Hardware polling code is based on code from [vcgencmd](https://github.com/raspberrypi/utils/tree/master/vcgencmd), however, it has been re-written in C++ with expanded features.

I was dissatisfied with existing tools (lm-sensors, xsensors, htop, vcgencmd) for quickly monitoring the hardware of my Pi during overclocking sessions.
I wanted a standalone command-line tool to be able to see voltage, frequencies, and temperatures. It was later expanded to a GTK3 GUI version and separate `libraspimon` library.

## Usage

__raspimon__ and __raspimon-gui__ require `sudo` permissions, or adding yourself to the `video` group like so:

```bash
sudo usermod -aG video $USER
```

Note that the group change takes effect at your next login, so log out and back in first.

To launch:

```bash
./raspimon -t 2 # Refresh every 2 seconds (default 1 sec.)
./raspimon -f   # Display temperatures in Fahrenheit
./raspimon -v   # Show program version
./raspimon -h   # Show help.

# OR for the GTK3 GUI version

./raspimon-gui
```

## Building

raspimon supports regular [`make`](./Makefile), as well as [`cmake`](./CMakeLists.txt) and [GN/Ninja](./BUILD.gn).

CMake requires version 3.10+, GN/Ninja requires my [gn-legacy](https://github.com/Alex313031/gn-legacy) repo.

```bash

# Standard GNU Make
make -j 4 # make with 4 jobs

make IS_DEBUG=1 # make a debug build

# CMake build
mkdir out && cd out && cmake ../
cmake --build . -j 4

# GN/Ninja build
ninja -C out/Default raspimon

# To build libraspimon as a shared .so instead of static library:
make SHARED_LIBRASPIMON=1 # GNU Make

cmake -DSHARED_LIBRASPIMON=ON ../ # CMake, from the out dir

gn gen out/Default --args="shared_libraspimon=true" # GN/Ninja, then build as usual
```

## License
This repository is licensed under the [BSD-3 Clause License](./LICENSE.md).
