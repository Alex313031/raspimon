// Copyright (c) 2026 Alex313031.

// Provides Linux terminal utility functions, and utility functions for
// the raspimon console frontend: ANSI colors, aligned printing, raw
// keyboard input, and signal cleanup

#include "console_utils.h"

// Whether to emit ANSI color codes; main() decides at startup
bool use_color = false;

namespace {
  // The terminal settings from before RawTerminal switched to raw input,
  // kept at file scope so HandleSignal() can also restore them
  termios orig_termios;
  bool termios_saved = false;
} // namespace

const char* Color(const char* color) {
  return use_color ? color : "";
}

void PrintOutHeader(std::ostream& out, const std::string& title) {
  std::string line = "--------" + title;
  line.resize(33, '-');
  out << Color(kColorHeader) << line << Color(kColorReset) << kEndLine;
}

void PrintOutEntry(std::ostream& out,
                   const std::string& name,
                   const std::string& value,
                   int width,
                   const char* value_color) {
  // The color codes go AROUND the setw() field, never inside it: setw
  // pads by byte count, and escape codes are bytes with zero visible
  // width, so a code inside the field would shrink the padding
  out << "  " << Color(kColorLabel) << std::left << std::setw(width) << name << Color(kColorReset)
      << ": " << Color(value_color) << value << Color(kColorReset) << kEndLine;
}

// Handles SIGINT (Ctrl+C) and SIGTERM (polite kill) - POSIX signals are
// the rough equivalent of a Win32 console control handler, except the
// handler runs by interrupting the program mid-instruction on its own
// stack. Because of that, only "async-signal-safe" functions are allowed
// here: raw syscalls like write(), but NOT std::cout (it might be halfway
// through a write, holding its internal lock, when the signal hits)
void HandleSignal(int signum) {
  // tcsetattr() is on POSIX's async-signal-safe list, so undoing raw mode
  // here is allowed (unlike std::cout)
  if (termios_saved) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  }
  // Leave the alternate screen buffer and restore the cursor in one raw
  // write (a no-op if the display loop never started)
  constexpr char kRestore[] = "\033[?1049l\033[?25h\n";
  ssize_t restored          = write(STDOUT_FILENO, kRestore, sizeof(kRestore) - 1);
  (void)restored;
  // The argument a signal handler receives is the signal number (SIGINT=2,
  // SIGTERM=15), not an exit code. Exit with the shell convention for
  // "quit by signal N" (128 + N, e.g. Ctrl+C -> 130) so scripts can tell
  // a signal quit from success (0) or a real failure (1). _exit() skips
  // destructors and stream flushing, the unsafe-in-a-handler parts of a
  // normal exit()
  _exit(128 + signum);
}

RawTerminal::RawTerminal() {
  if (!isatty(STDIN_FILENO)) {
    return; // stdin is a pipe/file: nothing to configure, keys can't arrive
  }
  if (tcgetattr(STDIN_FILENO, &orig_termios) != 0) {
    return;
  }
  termios raw   = orig_termios;
  termios_saved = true;
  // ICANON off = deliver bytes as they are typed instead of buffering a
  // whole line until Enter; ECHO off = don't print keys over the dashboard
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_cc[VMIN]  = 0; // read() may return with nothing...
  raw.c_cc[VTIME] = 0; // ...and never blocks (poll() does the waiting)
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

RawTerminal::~RawTerminal() {
  if (termios_saved) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    termios_saved = false;
  }
}

bool WaitForQuit(std::chrono::milliseconds delay) {
  if (!termios_saved) {
    // Not a tty (or raw mode failed): keys can't be read sensibly, so
    // behave like the old refresh loop and just sleep
    std::this_thread::sleep_for(delay);
    return false;
  }
  // Instead of sleeping, poll() stdin with the refresh delay as timeout:
  // it returns as soon as a key arrives or the time runs out, whichever
  // comes first (like WaitForSingleObject() on the console handle)
  const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + delay;
  for (;;) {
    const std::chrono::milliseconds remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                              std::chrono::steady_clock::now());
    if (remaining <= std::chrono::milliseconds::zero()) {
      return false;
    }
    pollfd request{STDIN_FILENO, POLLIN, 0};
    const int ready = poll(&request, 1, static_cast<int>(remaining.count()));
    if (ready < 0) {
      if (errno == EINTR) {
        continue; // a signal interrupted the wait: resume it
      }
      return false;
    }
    if (ready == 0) {
      return false; // timed out: run the next refresh
    }
    char key = 0;
    if (read(STDIN_FILENO, &key, 1) != 1) {
      return false; // EOF: stdin closed under us
    }
    if (key == 'q' || key == 'Q') {
      return true;
    }
    if (key == '\033') {
      // A lone Esc quits, but Esc is also how terminals encode special
      // keys (an arrow key arrives as the bytes ESC [ A): if more bytes
      // follow immediately, it's such a sequence - swallow and ignore it
      pollfd more{STDIN_FILENO, POLLIN, 0};
      if (poll(&more, 1, 0) <= 0) {
        return true;
      }
      std::array<char, 8> discard;
      ssize_t drained = read(STDIN_FILENO, discard.data(), discard.size());
      (void)drained;
    }
    // Any other key: ignore it and wait out the remaining time
  }
}
