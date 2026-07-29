#ifndef RASPIMON_UTILS_H_
#define RASPIMON_UTILS_H_

#include "pch.h"

// Whether to use debug mode, even in a release build, set by cmdline flags.
extern bool want_debug;

// The display is drawn with ANSI/VT escape sequences: control codes
// written to stdout that the terminal interprets instead of printing
// (Windows Terminal and modern conhost understand the same codes, as
// enabled by ENABLE_VIRTUAL_TERMINAL_PROCESSING). "\033" is ESC; the ones
// used here are:
//   ESC[?25l hide cursor    ESC[2J clear whole screen   ESC[H cursor home
//   ESC[K    erase to end of line                       ESC[?25h show cursor
//   ESC[J    erase from cursor to end of screen
//
// kEndLine ends a line with erase-to-end-of-line before the newline, so
// values that got shorter since the last frame (e.g. "1.875V" -> "1V")
// don't leave leftover characters on screen
inline constexpr char kEndLine[] = "\033[K\n";

// Compile-time strlen, for computing label column widths
constexpr size_t cstrlen(const char* in) {
  size_t len = 0;
  while (in[len] != '\0') {
    ++len;
  }
  return len;
}

// Numeric parsers for firmware responses; std::nullopt on malformed input
std::optional<long long> ParseInt(const std::string& in);
std::optional<double> ParseDouble(const std::string& in);

// Prints a section header padded with dashes to a fixed width
void PrintOutHeader(std::ostream& out, const std::string& title);

// Prints one "      name    : value" entry, padding `name` to `width` columns
void PrintOutEntry(std::ostream& out, const std::string& name, const std::string& value,
                   int width);

// Handles interrupt signals; `signum` is the number of the signal that fired
void HandleSignal(int signum);

// Whether to output extra debug information: true when either DEBUG/_DEBUG
// is defined (debug build) or the -d/--debug flag was passed.
bool IsDebugMode();

#endif // RASPIMON_UTILS_H_
