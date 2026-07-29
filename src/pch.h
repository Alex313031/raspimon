#ifndef RASPIMON_PCH_H_
#define RASPIMON_PCH_H_

// C++ Runtime Headers
#include <array>     // std::array
#include <chrono>    // std::chrono::milliseconds
#include <csignal>   // std::signal() and the SIG* constants
#include <cstdlib>   // EXIT_SUCCESS / EXIT_FAILURE
#include <cstring>   // std::memcpy(), strnlen()
#include <fstream>   // std::ifstream, for reading /proc files
#include <iomanip>   // std::setw(), std::setprecision()
#include <iostream>  // std::cout / std::cerr
#include <optional>  // std::optional
#include <sstream>   // std::ostringstream
#include <stdexcept> // std::runtime_error
#include <string>    // std::string
#include <thread>    // std::this_thread::sleep_for()
#include <utility>   // std::pair
#include <vector>    // std::vector

// Linux system headers
#include <fcntl.h>     // open() and its O_* access flags (like CreateFile() on Win32)
#include <getopt.h>    // getopt_long() and struct option, for parsing --long flags
#include <sys/ioctl.h> // ioctl(): device I/O control (like DeviceIoControl() on Win32)
#include <unistd.h>    // Core POSIX syscall wrappers: close(), write(), getopt()

// Convert compiler defines to usable bool
inline constexpr bool is_debug =
#if defined(DEBUG) || defined(_DEBUG)
    true;
#else
    false;
#endif // DEBUG || _DEBUG

#endif // RASPIMON_PCH_H_
