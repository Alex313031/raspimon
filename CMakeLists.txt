cmake_minimum_required(VERSION 3.10)

option(USE_LLVM "Compile with LLVM" OFF)

# Compile with LLVM instead of GCC. This must happen BEFORE project():
# that's where CMake detects the compiler and locks it into the cache, and
# changing CMAKE_<LANG>_COMPILER after detection is unsupported (it gets
# ignored, or triggers a cache wipe and reconfigure loop). Toggling
# USE_LLVM is best done on a fresh build directory for the same reason
if(USE_LLVM)
  set(CMAKE_C_COMPILER "clang" CACHE STRING "C Compiler" FORCE)
  set(CMAKE_C_COMPILER_AR "llvm-ar" CACHE STRING "C ar command" FORCE)
  set(CMAKE_C_COMPILER_RANLIB "llvm-ranlib" CACHE STRING "C ranlib command" FORCE)
  set(CMAKE_CXX_COMPILER "clang++" CACHE STRING "C++ Compiler" FORCE)
  set(CMAKE_CXX_COMPILER_AR "llvm-ar" CACHE STRING "C++ ar command" FORCE)
  set(CMAKE_CXX_COMPILER_RANLIB "llvm-ranlib" CACHE STRING "C++ ranlib command" FORCE)
  set(CMAKE_AR "llvm-ar" CACHE STRING "AR" FORCE)
  set(CMAKE_RANLIB "llvm-ranlib" CACHE STRING "ranlib command" FORCE)
  set(CMAKE_ADDR2LINE "llvm-addr2line" CACHE STRING "addr2line command" FORCE)
  set(CMAKE_OBJCOPY "llvm-objcopy" CACHE STRING "objcopy command" FORCE)
  set(CMAKE_OBJDUMP "llvm-objdump" CACHE STRING "objdump command" FORCE)
  set(CMAKE_READELF "llvm-readelf" CACHE STRING "readelf command" FORCE)
  set(CMAKE_STRIP "llvm-strip" CACHE STRING "strip command" FORCE)
  set(CMAKE_LINKER "lld" CACHE STRING "Linker" FORCE)
endif()

project(raspimon)

# Default to color
set(CMAKE_COLOR_MAKEFILE ON)

# Log compile commands to compile_commands.json in the build dir (Makefile
# and Ninja generators only). Must be set BEFORE targets are created:
# since CMake 3.20 each target snapshots this setting at creation time
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(IS_DEBUG "Compile in debug mode" OFF)
option(SHARED_LIBRASPIMON "Build libraspimon as a shared library" OFF)

# The library: all the VideoCore/vcgencmd and kernel hardware querying
if(SHARED_LIBRASPIMON)
  add_library(libraspimon SHARED
    src/libraspimon/gencmd.cc
    src/libraspimon/utils.cc
  )
  target_compile_definitions(libraspimon PUBLIC LIBRASPIMON_SHARED=1)
else()
  add_library(libraspimon STATIC
    src/libraspimon/gencmd.cc
    src/libraspimon/utils.cc
  )
endif()
# OUTPUT_NAME raspimon + the automatic "lib" prefix = libraspimon.a/.so
# (the target itself can't be named plain "raspimon" - the console
# executable owns that name)
set_target_properties(libraspimon PROPERTIES
  OUTPUT_NAME raspimon
  POSITION_INDEPENDENT_CODE ON
)
# PUBLIC: anything linking the library inherits the include path, so the
# frontends can #include <libraspimon.h>
target_include_directories(libraspimon PUBLIC src/libraspimon)

# The console frontend
add_executable(raspimon
  src/console/console_utils.cc
  src/console/raspimon.cc
)
target_include_directories(raspimon PRIVATE src/console)
target_link_libraries(raspimon libraspimon m)
if(SHARED_LIBRASPIMON)
  # $ORIGIN = "the directory the binary is in": a shipped raspimon +
  # libraspimon.so pair works side by side
  set_target_properties(raspimon PROPERTIES BUILD_RPATH "\$ORIGIN")
  target_compile_definitions(raspimon PRIVATE LIBRASPIMON_SHARED=1)
else()
  # Only the static-library console build can be fully static, like the
  # old single-file raspimon was (LINK_FLAGS, not target_link_options,
  # keeps the CMake 3.10 minimum honest)
  set_target_properties(raspimon PROPERTIES LINK_FLAGS "-static")
endif()

# The GTK3 frontend - only built when the GTK3 dev package is installed
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(GTK3 gtk+-3.0)
endif()
if(GTK3_FOUND)
  add_executable(raspimon-gui
    src/gui/gui_utils.cc
    src/gui/raspimon_gui.cc
  )
  target_include_directories(raspimon-gui PRIVATE src/gui ${GTK3_INCLUDE_DIRS})
  target_link_libraries(raspimon-gui libraspimon ${GTK3_LIBRARIES} m)
  if(SHARED_LIBRASPIMON)
    set_target_properties(raspimon-gui PROPERTIES BUILD_RPATH "\$ORIGIN")
    target_compile_definitions(raspimon-gui PRIVATE LIBRASPIMON_SHARED=1)
  endif()
else()
  message(WARNING "GTK3 dev files not found (libgtk-3-dev); skipping raspimon-gui")
endif()

# Offer the standard build types as a dropdown in cmake-gui/ccmake
if(NOT CMAKE_CONFIGURATION_TYPES)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Set default build type (Release, or Debug with -DIS_DEBUG=1) if empty
if(NOT CMAKE_BUILD_TYPE)
  if(IS_DEBUG)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose build type" FORCE)
  else()
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose build type" FORCE)
  endif()
endif()

# Always use C17/C++17. Note: no global -static (the GUI links GTK, which
# is shared-only); the console target sets it per-target above
string(APPEND CMAKE_C_FLAGS " -std=c17")
string(APPEND CMAKE_CXX_FLAGS " -std=c++17")

# Override default CFLAGS (future use)
set(CMAKE_C_FLAGS_DEBUG "-Wall -g2 -Og -DDEBUG -D_DEBUG" CACHE STRING "Forced Debug CFLAGS" FORCE)
set(CMAKE_C_FLAGS_RELEASE "-Wno-error -g0 -O3 -DNDEBUG -D_NDEBUG" CACHE STRING "Forced Release CFLAGS" FORCE)
set(CMAKE_C_FLAGS_MINSIZEREL "-Wno-error -g0 -Os -DNDEBUG -D_NDEBUG" CACHE STRING "Forced MinSizeRel CFLAGS" FORCE)
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-Wall -g1 -O2 -DNDEBUG -D_NDEBUG" CACHE STRING "Forced RelWithDebInfo CFLAGS" FORCE)

# Override default CXXFLAGS
set(CMAKE_CXX_FLAGS_DEBUG "-Wall -g2 -Og -DDEBUG -D_DEBUG" CACHE STRING "Forced Debug CXXFLAGS" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-Wno-error -g0 -O3 -DNDEBUG -D_NDEBUG" CACHE STRING "Forced Release CXXFLAGS" FORCE)
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Wno-error -g0 -Os -DNDEBUG -D_NDEBUG" CACHE STRING "Forced MinSizeRel CXXFLAGS" FORCE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Wall -g1 -O2 -DNDEBUG -D_NDEBUG" CACHE STRING "Forced RelWithDebInfo CXXFLAGS" FORCE)

# Override default LDFLAGS
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-Wl,-O0" CACHE STRING "Forced Debug LDFLAGS" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-Wl,-O3 -s" CACHE STRING "Forced Release LDFLAGS" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "-Wl,-O3 -s" CACHE STRING "Forced MinSizeRel LDFLAGS" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "-Wl,-O2" CACHE STRING "Forced RelWithDebInfo LDFLAGS" FORCE)
