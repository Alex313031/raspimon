#ifndef RASPIMON_RASPIMON_GUI_H_
#define RASPIMON_RASPIMON_GUI_H_

#include <libraspimon.h>

#include "pch.h"

// These next few lines are where we control the GUI frontend's own
// version number, independent of libraspimon's (the About dialog shows both).
// Adhere to semver -> semver.org
#define RASPIMON_GUI_MAJOR 1
#define RASPIMON_GUI_MINOR 1
#define RASPIMON_GUI_BUILD 2

#define COPYRIGHT_YEAR "2026" // For the About dialog

// Macro to convert to string
#if !defined(STRINGIZE)
 #define STRINGIZER_(in) #in
 #define STRINGIZE(in)   STRINGIZER_(in)
#endif // !defined(STRINGIZE)

// Main version constants
#ifndef VERSION_
 // Run stringizer above
 #define VERSION_(major, minor, build) STRINGIZE(major.minor.build)
#endif // VERSION_

// Version string
#define RASPIMON_GUI_VERSION VERSION_(RASPIMON_GUI_MAJOR, RASPIMON_GUI_MINOR, RASPIMON_GUI_BUILD)

inline constexpr char kAppName[] = "raspimon-gui"; // name of the app

inline constexpr unsigned int kDefaultDelayMs = 1000; // refresh delay, in ms.

// In the UTF-8 encoding Linux terminals speak, characters beyond ASCII
// are multi-byte sequences (the copyright sign is the two bytes 0xC2
// 0xA9), so these must be char arrays - they don't fit in a single `char`
inline constexpr char kCopyrightSymbol[] = "©"; // For the About menu, later
inline constexpr char kDegreeSymbol[]    = "°"; // For temperature output

// Default window width/height, sized so a full dashboard fits untruncated
inline constexpr int CW_WIDTH  = 460;
inline constexpr int CW_HEIGHT = 640;

// A sensor to display, as {display label, gencmd argument}
struct Sensor {
  const char* label;
  const char* arg;
};

// Fills `buffer` with one full dashboard frame read through `mbox`,
// replacing the previous contents - the GUI analog of the console's
// GetInfo(). Returns false if a VideoCore query failed
bool RenderDashboard(GtkTextBuffer* buffer, const Mbox& mbox);

#endif // RASPIMON_RASPIMON_GUI_H_
