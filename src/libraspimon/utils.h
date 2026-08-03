#ifndef LIBRASPIMON_UTILS_H_
#define LIBRASPIMON_UTILS_H_

#include "pch.h"

// Whether to use debug mode, even in a release build, set by frontends
// (e.g. from a -d/--debug flag).
extern bool want_debug;

// Whether to output extra debug information: true when either DEBUG/_DEBUG
// is defined (debug build) or want_debug was set.
bool IsDebugMode();

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

// Latches the board revision code (from Mbox::GetBoardRevision()) so the
// helpers below can decode it; call once at startup
void SetBoardRevision(unsigned int revision);

// Which Pi generation this is, decided by SoC: BCM2836 = Pi 2,
// BCM2837 = Pi 3 (also late Pi 2s and the Zero 2 W), BCM2711 = Pi 4
// family, BCM2712 = Pi 5 family. All false until SetBoardRevision() is
// called with a recognized revision
bool IsPi2();
bool IsPi3();
bool IsPi4();
bool IsPi5();

// Human-readable model + RAM decoded from the revision code, e.g.
// "Pi Model 5 8GB"; "(Unknown Model)" if the revision was never set
std::string GetPiModelName();

// System RAM as the kernel sees it, in megabytes. `available_mb` is the
// kernel's estimate of how much memory apps could still allocate without
// swapping (more honest than "free", which ignores reclaimable caches)
struct MemInfo {
  long long total_mb;
  long long available_mb;
};

// Reads MemInfo from /proc/meminfo; std::nullopt if it can't be parsed
std::optional<MemInfo> GetKernelMemInfo();

// Reads the fan tachometer (RPM) from the kernel's hwmon interface - the
// fan on the Pi 5's dedicated fan header; std::nullopt if no fan is there
std::optional<long long> GetFanRpm();

#endif // LIBRASPIMON_UTILS_H_
