#ifndef RASPIMON_RASPIMON_GUI_H_
#define RASPIMON_RASPIMON_GUI_H_

#include "pch.h"

#include <libraspimon.h>

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
