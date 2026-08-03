#ifndef LIBRASPIMON_LIBRASPIMON_H_
#define LIBRASPIMON_LIBRASPIMON_H_

// Public umbrella header for libraspimon: querying the VideoCore firmware
// (mailbox/gencmd) and the kernel's hardware interfaces. Consumers should
// include only this header.

// These next few lines are where we control the version number, which is
// global: the frontends report the library's version as their own.
// Adhere to semver -> semver.org
#define MAJOR_VERSION 1
#define MINOR_VERSION 1
#define BUILD_VERSION 1

#define COPYRIGHT_YEAR "2026" // For ShowVersion()

// Macro to convert to string
#if !defined(STRINGIZE)
 #define STRINGIZER_(in) #in
 #define STRINGIZE(in)   STRINGIZER_(in)
#endif // !defined(STRINGIZE)

// Main version constants
#ifndef VERSION_
 // Run stringizer above
 #define VERSION_(major, minor, build) STRINGIZE(major.minor.build)
 // Version string
 #define RASPIMON_VERSION_STRING VERSION_(MAJOR_VERSION, MINOR_VERSION, BUILD_VERSION)
#endif // VERSION_

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
