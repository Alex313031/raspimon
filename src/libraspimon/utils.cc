// Copyright (c) 2026 Alex313031

// General utility functions for libraspimon: board identification and
// kernel-side (/proc, /sys) hardware readings

#include "utils.h"

#include "libraspimon.h" // for LIBRASPIMON_VERSION_STRING

// Defined here so it lives with its extern declaration in utils.h; set by
// frontends (e.g. when -d/--debug is passed)
bool want_debug = false;

namespace {
  // The board revision code, pushed in by the frontend at startup. It's
  // the "new-style" bitfield (same value as /proc/cpuinfo's Revision line):
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
} // namespace

bool IsDebugMode() {
  return is_debug || want_debug;
}

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
    // This runs once per refresh frame, so keep the complaint behind -d
    if (IsDebugMode()) {
      std::cerr << __func__ << " reported no board revision!" << std::endl;
    }
    return "(Unknown Model)";
  }
  const char* model;
  switch ((board_revision >> 4) & 0xFF) { // model type field
    case 0x04:
      model = "2B";
      break;
    case 0x08:
      model = "3B";
      break;
    case 0x09:
      model = "Zero";
      break;
    case 0x0a:
      model = "CM3";
      break;
    case 0x0c:
      model = "Zero W";
      break;
    case 0x0d:
      model = "3B+";
      break;
    case 0x0e:
      model = "3A+";
      break;
    case 0x10:
      model = "CM3+";
      break;
    case 0x11:
      model = "4B";
      break;
    case 0x12:
      model = "Zero 2 W";
      break;
    case 0x13:
      model = "400";
      break;
    case 0x14:
      model = "CM4";
      break;
    case 0x15:
      model = "CM4S";
      break;
    case 0x17:
      model = "5";
      break;
    case 0x18:
      model = "CM5";
      break;
    case 0x19:
      model = "500";
      break;
    case 0x1a:
      model = "CM5 Lite";
      break;
    default:
      model = "(Unknown Model)";
      break;
  }
  // RAM size field: 0 = 256MB doubling each step up to 6 = 16GB
  constexpr std::array<const char*, 7> kRamSizes{"256MB", "512MB", "1GB", "2GB",
                                                 "4GB",   "8GB",   "16GB"};
  std::string name       = std::string("Pi Model ") + model;
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

std::optional<long long> GetFanRpm() {
  // The fan's tachometer is exposed by the kernel's hwmon subsystem as a
  // text file holding the RPM. The hwmonN directory in the middle of the
  // path varies per boot, so expand the wildcard the way the shell would
  glob_t matches{};
  std::optional<long long> rpm;
  if (glob("/sys/devices/platform/cooling_fan/hwmon/*/fan1_input", 0, nullptr, &matches) == 0 &&
      matches.gl_pathc > 0) {
    std::ifstream fan(matches.gl_pathv[0]);
    std::string line;
    if (fan.is_open() && std::getline(fan, line)) {
      rpm = ParseInt(line);
    }
  }
  globfree(&matches); // required even when glob() found nothing
  return rpm;
}

const char* GetLibRaspiMonVersion() {
  return LIBRASPIMON_VERSION_STRING;
}
