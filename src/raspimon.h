#ifndef RASPIMON_H_
#define RASPIMON_H_

// C++ Runtime Headers
#include <array> // std::array
#include <chrono> // std::chrono::milliseconds
#include <csignal> // std::signal() and the SIG* constants
#include <cstdlib> // EXIT_SUCCESS / EXIT_FAILURE
#include <cstring> // std::memcpy(), strnlen()
#include <iostream> // std::cout / std::cerr
#include <iomanip> // std::setw(), std::setprecision()
#include <optional> // std::optional
#include <sstream> // std::ostringstream
#include <stdexcept> // std::runtime_error
#include <string> // std::string
#include <thread> // std::this_thread::sleep_for()

// Linux system headers
#include <unistd.h> // Core POSIX syscall wrappers: close(), write(), getopt()
#include <fcntl.h> // open() and its O_* access flags (like CreateFile() on Win32)
#include <sys/ioctl.h> // ioctl(): device I/O control (like DeviceIoControl() on Win32)

// These next few lines are where we control version number
// Adhere to semver -> semver.org
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define BUILD_VERSION 2

// Macro to convert to string
#if !defined(STRINGIZE)
 #define STRINGIZER_(in) #in
 #define STRINGIZE(in) STRINGIZER_(in)
#endif // !defined(STRINGIZE)

// Main version constants
#ifndef VERSION_
 // Run stringizer above
 #define VERSION_(major,minor,build) STRINGIZE(major.minor.build)
 // Version string
 #define VERSION_STRING VERSION_(MAJOR_VERSION, MINOR_VERSION, BUILD_VERSION)
#endif // VERSION_

// Mailbox "tag id" that tells the VideoCore firmware to execute a gencmd
// command (the text commands vcgencmd uses, e.g. "measure_temp")
inline constexpr unsigned int kGetGencmdResult = 0x00030080; // tag id

// The ioctl request code the vcio kernel driver expects. _IOWR() packs the
// transfer direction (read+write), the driver's magic number (100), a
// command number (0), and the argument size into one integer - Linux's
// version of the Win32 CTL_CODE() macro for building IOCTL_* codes
inline constexpr unsigned long kIoctlMboxProperty = _IOWR(100, 0, char *); // for accessing mbox

inline constexpr size_t kMaxString = 4u * 1024u; // Max command/response string len

inline constexpr unsigned long kDefaultDelay = 1000UL; // default delay, 1000ms

inline const char kAppName[] = "raspimon"; // name of the app

// RAII wrapper around the VideoCore mailbox char device: the constructor
// opens the device and the destructor closes it, so holding an Mbox object
// is holding the open device (no separate init/cleanup calls to forget)
class Mbox {
 public:
  // Opens the mbox device; throws std::runtime_error if unavailable
  Mbox();
  ~Mbox();

  Mbox(const Mbox&) = delete;
  Mbox& operator=(const Mbox&) = delete;

  // Roughly equivalent to vcgencmd: sends `command` to the VideoCore and
  // returns the raw response (e.g. "frequency(48)=600000000"), or
  // std::nullopt on failure
  std::optional<std::string> gencmd(const std::string& command) const;

 private:
  // use ioctl to send mbox property message
  int property(void *buf) const;

  // The "file descriptor" from open(): a small integer handle to the open
  // device, POSIX's equivalent of a Win32 HANDLE. -1 means "not open"
  // (POSIX calls that fail return -1, not NULL/INVALID_HANDLE_VALUE)
  int fd_ = -1;
};

// Shows usage help message.
void show_help();

// Collects system info and displays it once.
bool get_info(const Mbox& mbox);

// Displays output, refreshing periodically every `delay`
void refresh_output(const Mbox& mbox, std::chrono::milliseconds delay);

#endif // RASPIMON_H_
