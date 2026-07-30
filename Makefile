# Copyright (c) 2026 Alex313031

# raspimon Makefile for gcc

SRCDIR  := src
TARGET  := raspimon

OBJECTS := raspimon.o gencmd.o utils.o
HEADERS := $(SRCDIR)/raspimon.h $(SRCDIR)/gencmd.h $(SRCDIR)/utils.h $(SRCDIR)/pch.h

# Compiler toolchain defaults
ifeq ($(USE_LLVM),1)
  CC   := clang
  CXX  := clang++
  AR   := llvm-ar
  LD   := lld
else
  CC   := gcc
  CXX  := g++
  AR   := ar
  LD   := ld
endif

ifeq ($(IS_DEBUG),1)
  CPPFLAGS += -DDEBUG -D_DEBUG
  CFLAGS   += -Wall -Og -g2
  CXXFLAGS += -std=c++17
  LDFLAGS  += -static
else
  CPPFLAGS += -DNDEBUG -D_NDEBUG
  CFLAGS   += -Wno-error -O2 -g0
  CXXFLAGS += -std=c++17
  LDFLAGS  += -static -s
endif

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) -lm

# Pattern rule: each foo.o is built from its matching src/foo.cc; `$<` is
# the first prerequisite, i.e. the one .cc file the % stem selected
%.o: $(SRCDIR)/%.cc $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	$(RM) $(TARGET) $(OBJECTS)

.PHONY: all clean
