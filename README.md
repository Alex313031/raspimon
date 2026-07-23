# raspimon

A command line hardware monitor for Raspberry Pi 2/3/4/5.  
It displays clocks, temperatures, clock speeds, and more at a glance, refreshing periodically.

## About
It is based on code from [vcgencmd](https://github.com/raspberrypi/utils/tree/master/vcgencmd), however, it has been re-written in C++ with expanded features.

I was dissatisfied with existing tools (lm-sensors, xsensors, htop, vcgencmd) for quickly monitoring the hardware of my Pi during overclocking sessions.
I wanted a standalone command-line tool to be able to see voltage, frequencies, and temperatures.

## Usage

```bash
raspimon -t 2 # Refresh every 2 seconds (default 1 sec.)
raspimon -f   # Display temperatures in Fahrenheit
raspimon -v   # Show program version
raspimon -h   # Show help.
```

## Building

raspimon supports regular [`make`](./Makefile), as well as [`cmake`](./CMakeLists.txt) and [GN/Ninja](./BUILD.gn).

CMake requires version 3.10+, GN/Ninja requires my [gn-legacy](https://github.com/Alex313031/gn-legacy) repo.

```bash

make -j 4 # make with 4 jobs

make IS_DEBUG=1 # make a debug build

mkdir out && cd out && cmake ../ # CMake build

ninja -C out/Default raspimon # Ninja build
```

## License
This repository is licensed under the [BSD-3 Clause License](./LICENSE.md).
