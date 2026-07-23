#ifndef RASPIMON_H_
#define RASPIMON_H_

// C++ Runtime Headers
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

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
#endif // !defined(_STRINGIZER_)

// Main version constants
#ifndef VERSION_
 // Run stringizer above
 #define VERSION_(major,minor,build) STRINGIZE(major.minor.build)
 // Version string
 #define VERSION_STRING VERSION_(MAJOR_VERSION, MINOR_VERSION, BUILD_VERSION)
#endif // _VERSION

#define GET_GENCMD_RESULT 0x00030080 // tag id

#define IOCTL_MBOX_PROPERTY _IOWR(100, 0, char *) // Static define for accessing mbox

#define MAX_STRING (4*1024) // Max output string len

extern unsigned long kDefaultDelay; // default delay

inline const char kAppName[] = "raspimon"; // name of the app

// Roughly equivalent to vcgencmd
unsigned int gencmd(int file_desc, const char *command, char *result, int result_len);

// Shows usage help message.
void show_help();

// Collects system info for display.
bool get_info();

// Displays output, refreshing periodically every `delay` milliseconds
void refresh_output(const unsigned long delay);

#endif // RASPIMON_H_
