// Copyright (c) 2026 Alex313031

// General utility functions

#include "utils.h"

// Defined here so it lives with its extern declaration in utils.h; set by
// ParseOptions() when -d/--debug is passed
bool want_debug = false;

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
  ssize_t restored = write(STDOUT_FILENO, "\033[?25h\n", 7);
  (void)restored;
  // The argument a signal handler receives is the signal number (SIGINT=2,
  // SIGTERM=15), not an exit code. Exit with the shell convention for
  // "quit by signal N" (128 + N, e.g. Ctrl+C -> 130) so scripts can tell
  // a signal quit from success (0) or a real failure (1)
  _exit(128 + signum);
}

bool IsDebugMode() {
  return is_debug || want_debug;
}
