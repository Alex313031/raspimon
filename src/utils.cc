// Copyright (c) 2026 Alex313031

// General utility functions

#include "utils.h"

// Defined here so it lives with its extern declaration in utils.h; set by
// ParseOptions() when -d/--debug is passed
bool want_debug = false;

namespace {
  // The board revision code, pushed in by main() at startup. It's the
  // "new-style" bitfield (same value as /proc/cpuinfo's Revision line):
  //   bits 0-3   board revision      bits 12-15 processor (SoC)
  //   bits 4-11  model type          bits 20-22 RAM size
  //   bit 23     set = new-style code (set on every Pi 2 and later)
  unsigned int board_revision = 0;

  // The SoC id is the cleanest "generation" signal: what the firmware can
  // and can't do tracks the chip, not the marketing name. 0 = unknown
  unsigned int ProcessorId() {
    if (!(board_revision & 0x800000)) { // old-style code: pre-Pi 2 board
      return 0;
    }
    return (board_revision >> 12) & 0xF;
  }

  // The terminal settings from before RawTerminal switched to raw input,
  // kept at file scope so HandleSignal() can also restore them
  termios orig_termios;
  bool termios_saved = false;
} // namespace

std::optional<long long> ParseInt(const std::string& in) {
  try {
    return std::stoll(in);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::optional<double> ParseDouble(const std::string& in) {
  try {
    return std::stod(in);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

void PrintOutHeader(std::ostream& out, const std::string& title) {
  std::string line = "--------" + title;
  line.resize(33, '-');
  out << line << kEndLine;
}

void PrintOutEntry(std::ostream& out, const std::string& name, const std::string& value,
                   int width) {
  out << "      " << std::left << std::setw(width) << name << ": " << value << kEndLine;
}

// Handles SIGINT (Ctrl+C) and SIGTERM (polite kill) - POSIX signals are
// the rough equivalent of a Win32 console control handler, except the
// handler runs by interrupting the program mid-instruction on its own
// stack. Because of that, only "async-signal-safe" functions are allowed
// here: raw syscalls like write(), but NOT std::cout (it might be halfway
// through a write, holding its internal lock, when the signal hits).
// Restores the cursor with the raw write() syscall (like WriteFile() to
// the stdout handle), then _exit() ends the process immediately without
// running destructors or flushing streams - the unsafe-in-a-handler parts
// of a normal exit()
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
  // a signal quit from success (0) or a real failure (1)
  _exit(128 + signum);
}

RawTerminal::RawTerminal() {
  if (!isatty(STDIN_FILENO)) {
    return; // stdin is a pipe/file: nothing to configure, keys can't arrive
  }
  if (tcgetattr(STDIN_FILENO, &orig_termios) != 0) {
    return;
  }
  termios raw    = orig_termios;
  termios_saved  = true;
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
  const std::chrono::steady_clock::time_point deadline =
      std::chrono::steady_clock::now() + delay;
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

bool IsDebugMode() {
  return is_debug || want_debug;
}

void SetBoardRevision(unsigned int revision) {
  board_revision = revision;
}

bool IsPi2() {
  return ProcessorId() == 1; // BCM2836
}

bool IsPi3() {
  return ProcessorId() == 2; // BCM2837
}

bool IsPi4() {
  return ProcessorId() == 3; // BCM2711
}

bool IsPi5() {
  return ProcessorId() == 4; // BCM2712
}

std::string GetPiModelName() {
  if (!(board_revision & 0x800000)) {
    if (IsDebugMode()) {
      std::cerr << __func__ << " reported no board revision!" << std::endl;
    }
    return "(Unknown Model)";
  }
  const char* model;
  switch ((board_revision >> 4) & 0xFF) { // model type field
    case 0x04: model = "2B"; break;
    case 0x08: model = "3B"; break;
    case 0x09: model = "Zero"; break;
    case 0x0a: model = "CM3"; break;
    case 0x0c: model = "Zero W"; break;
    case 0x0d: model = "3B+"; break;
    case 0x0e: model = "3A+"; break;
    case 0x10: model = "CM3+"; break;
    case 0x11: model = "4B"; break;
    case 0x12: model = "Zero 2 W"; break;
    case 0x13: model = "400"; break;
    case 0x14: model = "CM4"; break;
    case 0x15: model = "CM4S"; break;
    case 0x17: model = "5"; break;
    case 0x18: model = "CM5"; break;
    case 0x19: model = "500"; break;
    case 0x1a: model = "CM5 Lite"; break;
    default:   model = "(Unknown Model)"; break;
  }
  // RAM size field: 0 = 256MB doubling each step up to 6 = 16GB
  constexpr std::array<const char*, 7> kRamSizes{"256MB", "512MB", "1GB", "2GB",
                                                 "4GB",   "8GB",   "16GB"};
  std::string name = std::string(" Pi Model ") + model;
  const unsigned int ram = (board_revision >> 20) & 0x7;
  if (ram < kRamSizes.size()) {
    name += std::string(" ") + kRamSizes[ram];
  }
  return name;
}

std::optional<MemInfo> GetKernelMemInfo() {
  // /proc/meminfo is a kernel-generated text file (procfs "files" are
  // views into live kernel state, not data on disk) with lines like
  //   MemTotal:        8058172 kB
  std::ifstream meminfo("/proc/meminfo");
  if (!meminfo.is_open()) {
    return std::nullopt;
  }
  std::optional<long long> total_kb;
  std::optional<long long> available_kb;
  std::string line;
  while (std::getline(meminfo, line) && (!total_kb || !available_kb)) {
    // rfind(prefix, 0) == 0 is the C++17 idiom for "starts with"; ParseInt
    // skips the leading spaces and stops at the " kB" suffix on its own
    if (line.rfind("MemTotal:", 0) == 0) {
      total_kb = ParseInt(line.substr(9));
    } else if (line.rfind("MemAvailable:", 0) == 0) {
      available_kb = ParseInt(line.substr(13));
    }
  }
  if (!total_kb || !available_kb) {
    return std::nullopt;
  }
  return MemInfo{*total_kb / 1024, *available_kb / 1024};
}
