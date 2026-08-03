#ifndef RASPIMON_PCH_H_
#define RASPIMON_PCH_H_

// The console frontend's precompiled header: terminal, command line, and
// formatting machinery. Hardware access comes from libraspimon (whose own
// pch also defines `is_debug` - do not redefine it here, both pchs end up
// in every console translation unit)

// C++ Runtime Headers
#include <array>     // std::array
#include <cerrno>    // errno and the E* error constants (like GetLastError() codes)
#include <chrono>    // std::chrono::milliseconds
#include <csignal>   // std::signal() and the SIG* constants
#include <cstdlib>   // EXIT_SUCCESS / EXIT_FAILURE, std::getenv()
#include <cstring>   // std::strcmp()
#include <iomanip>   // std::setw(), std::setprecision()
#include <iostream>  // std::cout / std::cerr
#include <optional>  // std::optional
#include <sstream>   // std::ostringstream / std::istringstream
#include <string>    // std::string
#include <thread>    // std::this_thread::sleep_for()
#include <utility>   // std::pair
#include <vector>    // std::vector

// Linux system headers
#include <getopt.h>    // getopt_long() and struct option, for parsing --long flags
#include <poll.h>      // poll(): wait for fd readiness with a timeout (like WaitForSingleObject())
#include <sys/ioctl.h> // ioctl(): TIOCGWINSZ terminal size (like GetConsoleScreenBufferInfo())
#include <termios.h>   // tcgetattr()/tcsetattr(): terminal input modes (like SetConsoleMode())
#include <unistd.h>    // Core POSIX syscall wrappers: write(), isatty(), getopt()

#endif // RASPIMON_PCH_H_
