#ifndef RASPIMON_CONSOLE_UTILS_H_
#define RASPIMON_CONSOLE_UTILS_H_

#include "pch.h"

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

// Whether to emit ANSI color codes; decided once at startup (on when
// stdout is a terminal, vetoed by the NO_COLOR convention or --no-color)
extern bool use_color;

// The dashboard's color scheme, as SGR ("Select Graphic Rendition")
// escape codes - ESC[<attributes>m, where 1 = bold and 30-37/90-97 pick
// the foreground color. Named by role, so retheming is a one-line change
inline constexpr char kColorReset[]  = "\033[0m";    // back to defaults
inline constexpr char kColorHeader[] = "\033[1;32m"; // bold green
inline constexpr char kColorLabel[]  = "\033[1;37m"; // bold white
inline constexpr char kColorValue[]  = "\033[1;36m"; // bold cyan
inline constexpr char kColorWarn[]   = "\033[1;33m"; // bold yellow
inline constexpr char kColorAlert[]  = "\033[1;31m"; // bold red

// Returns `color` when color is on and "" when off, so call sites can
// stream it unconditionally
const char* Color(const char* color);

// Prints a section header padded with dashes to a fixed width
void PrintOutHeader(std::ostream& out, const std::string& title);

// Prints one "  name    : value" entry, padding `name` to `width` columns.
// `value_color` tints the value; pass kColorWarn/kColorAlert to make a
// reading stand out (temperature does this by threshold)
void PrintOutEntry(std::ostream& out,
                   const std::string& name,
                   const std::string& value,
                   int width,
                   const char* value_color = kColorValue);

// Handles interrupt signals; `signum` is the number of the signal that fired
void HandleSignal(int signum);

// RAII wrapper that switches the terminal to non-canonical, no-echo input
// for its lifetime, so single keypresses arrive immediately (no Enter
// needed) and aren't echoed over the dashboard - the POSIX equivalent of
// Win32 SetConsoleMode() clearing ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT.
// The destructor restores the previous settings; HandleSignal() restores
// them too, covering the Ctrl+C path. Does nothing if stdin isn't a tty
class RawTerminal {
 public:
  RawTerminal();
  ~RawTerminal();

  RawTerminal(const RawTerminal&)            = delete;
  RawTerminal& operator=(const RawTerminal&) = delete;
};

// Waits up to `delay`, returning early with true if Q or Esc was pressed
// (false = the delay elapsed, keep running). Falls back to a plain sleep
// when stdin isn't a tty in raw mode
bool WaitForQuit(std::chrono::milliseconds delay);

#endif // RASPIMON_CONSOLE_UTILS_H_
