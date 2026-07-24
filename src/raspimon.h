#ifndef RASPIMON_H_
#define RASPIMON_H_

// C++ Runtime Headers
#include <array>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

// Linux system headers
#include <unistd.h> // posix
#include <fcntl.h> // open
#include <sys/ioctl.h> // ioctl

// These next few lines are where we control version number
// Adhere to semver -> semver.org
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define BUILD_VERSION 1

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

inline constexpr unsigned int kGetGencmdResult = 0x00030080; // tag id

inline constexpr unsigned long kIoctlMboxProperty = _IOWR(100, 0, char *); // for accessing mbox

inline constexpr size_t kMaxString = 4u * 1024u; // Max command/response string len

inline constexpr std::chrono::milliseconds kDefaultDelay{1000}; // default delay

inline const char kAppName[] = "raspimon"; // name of the app

// RAII wrapper around the VideoCore mailbox char device
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

  int fd_ = -1;
};

// Shows usage help message.
void show_help();

// Collects system info and displays it once.
bool get_info(const Mbox& mbox);

// Displays output, refreshing periodically every `delay`
void refresh_output(const Mbox& mbox, std::chrono::milliseconds delay);

#endif // RASPIMON_H_
