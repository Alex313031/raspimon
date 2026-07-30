#ifndef RASPIMON_RASPIMON_H_
#define RASPIMON_RASPIMON_H_

#include "gencmd.h"
#include "pch.h"

// These next few lines are where we control version number
// Adhere to semver -> semver.org
#define MAJOR_VERSION 1
#define MINOR_VERSION 0
#define BUILD_VERSION 9

#define COPYRIGHT_YEAR "2026" // For ShowVersion()

// Macro to convert to string
#if !defined(STRINGIZE)
 #define STRINGIZER_(in) #in
 #define STRINGIZE(in) STRINGIZER_(in)
#endif // !defined(STRINGIZE)

// Main version constants
#ifndef VERSION_
 // Run stringizer above
 #define VERSION_(major, minor, build) STRINGIZE(major.minor.build)
 // Version string
 #define VERSION_STRING VERSION_(MAJOR_VERSION, MINOR_VERSION, BUILD_VERSION)
#endif // VERSION_

inline constexpr char kAppName[] = "raspimon"; // name of the app

inline constexpr unsigned long kDefaultDelay = 1000UL; // default delay, 1000ms.

// In the UTF-8 encoding Linux terminals speak, characters beyond ASCII
// are multi-byte sequences (the copyright sign is the two bytes 0xC2
// 0xA9), so these must be char arrays - they don't fit in a single `char`
inline constexpr char kCopyrightSymbol[] ="\u00A9"; // The © symbol
inline constexpr char kDegreeSymbol[] ="\u00B0"; // For temperature output

// A sensor to display, as {display label, gencmd argument}
struct Sensor {
  const char* label;
  const char* arg;
};

// Parses the command line, applying flag side effects (like -f) and filling
// in `delay`. Returns std::nullopt if the program should keep running, or
// the process exit code to quit with (after -h/-v, or on an invalid flag)
std::optional<int> ParseOptions(int argc, char* argv[], std::chrono::milliseconds& delay);

// Shows usage help message.
void ShowHelp();

// Shows version info.
void ShowVersion();

// Collects system info and displays it once, printing at most `max_lines`
// lines (0 = no limit), so a frame taller than the terminal window can be
// cut off instead of making the terminal scroll.
bool GetInfo(const Mbox& mbox, int max_lines = 0);

// Displays output, refreshing periodically every `delay`. Returns false
// if a VideoCore query failed and the display loop had to stop
bool RefreshTermOutput(const Mbox& mbox, std::chrono::milliseconds delay);

#endif // RASPIMON_RASPIMON_H_
