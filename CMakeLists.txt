cmake_minimum_required(VERSION 3.10)
project(raspimon)

option(IS_DEBUG "Compile in debug mode" OFF)

add_executable(raspimon src/raspimon.cc)

target_link_libraries(raspimon m)

if(IS_DEBUG)
	target_compile_definitions(raspimon PRIVATE DEBUG _DEBUG)
  target_compile_options(raspimon PRIVATE -Wall -Og -g2 -std=c++17)
  target_link_options(raspimon PRIVATE -static)
else()
  target_compile_definitions(raspimon PRIVATE NDEBUG _NDEBUG)
  target_compile_options(raspimon PRIVATE -Wno-error -O2 -g0 -std=c++17)
  target_link_options(raspimon PRIVATE -static -s)
endif()
