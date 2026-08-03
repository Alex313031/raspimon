#ifndef LIBRASPIMON_PCH_H_
#define LIBRASPIMON_PCH_H_

// libraspimon's own precompiled header: only what the library itself
// needs. The frontends have their own, slimmer or fatter as they require

// C++ Runtime Headers
#include <array>     // std::array
#include <cstring>   // std::memcpy(), strnlen()
#include <fstream>   // std::ifstream, for reading /proc and /sys files
#include <iostream>  // std::cerr (debug/error output)
#include <optional>  // std::optional
#include <stdexcept> // std::runtime_error
#include <string>    // std::string

// Linux system headers
#include <fcntl.h>     // open() and its O_* access flags (like CreateFile() on Win32)
#include <glob.h>      // glob(): expand shell wildcards (like FindFirstFile() on Win32)
#include <sys/ioctl.h> // ioctl(): device I/O control (like DeviceIoControl() on Win32)
#include <unistd.h>    // Core POSIX syscall wrappers: close(), write()

// Convert compiler defines to usable bool. Defined here (and only here -
// the frontend pchs must not redefine it, since library headers pull this
// pch into every frontend translation unit too)
inline constexpr bool is_debug =
#if defined(DEBUG) || defined(_DEBUG)
    true;
#else
    false;
#endif // DEBUG || _DEBUG

#endif // LIBRASPIMON_PCH_H_
