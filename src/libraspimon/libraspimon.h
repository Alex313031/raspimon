#ifndef LIBRASPIMON_LIBRASPIMON_H_
#define LIBRASPIMON_LIBRASPIMON_H_

// Public umbrella header for libraspimon: querying the VideoCore firmware
// (mailbox/gencmd) and the kernel's hardware interfaces. Consumers should
// include only this header.

// These next few lines are where we control the library's own version
// number - each frontend now carries its own, and can report ours at
// runtime via GetLibRaspiMonVersion().
// Adhere to semver -> semver.org
#define LIBRASPIMON_MAJOR_VERSION 1
#define LIBRASPIMON_MINOR_VERSION 1
#define LIBRASPIMON_BUILD_VERSION 1

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
#define LIBRASPIMON_VERSION_STRING \
  VERSION_(LIBRASPIMON_MAJOR_VERSION, LIBRASPIMON_MINOR_VERSION, LIBRASPIMON_BUILD_VERSION)

// When built as a shared library, public symbols get default visibility
// explicitly (a no-op unless -fvisibility=hidden is ever turned on)
#if defined(LIBRASPIMON_SHARED)
 #define LIB_EXPORT __attribute__((visibility("default")))
#else
 #define LIB_EXPORT
#endif // defined(LIBRASPIMON_SHARED)

#include "gencmd.h"
#include "utils.h"

#endif // LIBRASPIMON_LIBRASPIMON_H_
