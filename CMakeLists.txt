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
# and Ninja generators only). Must be set BEFORE add_executable(): since
# CMake 3.20 each target snapshots this setting at the moment it is created
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sources
add_executable(raspimon
  src/gencmd.cc
  src/raspimon.cc
  src/utils.cc
)

option(IS_DEBUG "Compile in debug mode" OFF)

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

# Always use C17/C++17 and make statically linked builds
string(APPEND CMAKE_C_FLAGS " -std=c17")
string(APPEND CMAKE_CXX_FLAGS " -std=c++17")
string(APPEND CMAKE_EXE_LINKER_FLAGS " -static")

# Linking is driven through the compiler, not by invoking the linker
# directly, so CMAKE_LINKER would go unused - instead ask clang to use lld
if(USE_LLVM)
  string(APPEND CMAKE_EXE_LINKER_FLAGS " -fuse-ld=lld")
endif()

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

# Libraries to link in
target_link_libraries(raspimon m)
