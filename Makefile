# Copyright (c) 2026 Alex313031

# raspimon Makefile for gcc: builds libraspimon (static by default, shared
# with SHARED_LIBRASPIMON=1), the console frontend `raspimon`, and the
# GTK3 frontend `raspimon-gui` (needs libgtk-3-dev installed)

LIBDIR := src/libraspimon
CONDIR := src/console
GUIDIR := src/gui

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

# Note: no global -static - the GUI can't be (GTK is shared-only), and the
# console binary adds it itself below when linking the static library
ifeq ($(IS_DEBUG),1)
  CPPFLAGS += -DDEBUG -D_DEBUG
  CFLAGS   += -Wall -Og -g2
  CXXFLAGS += -std=c++17
else
  CPPFLAGS += -DNDEBUG -D_NDEBUG
  CFLAGS   += -Wno-error -O2 -g0
  CXXFLAGS += -std=c++17
  LDFLAGS  += -s
endif

CPPFLAGS += -I$(LIBDIR)

LIB_OBJECTS := gencmd.o utils.o
CON_OBJECTS := raspimon.o console_utils.o
GUI_OBJECTS := raspimon_gui.o gui_utils.o

LIB_HEADERS := $(LIBDIR)/libraspimon.h $(LIBDIR)/gencmd.h $(LIBDIR)/utils.h $(LIBDIR)/pch.h
CON_HEADERS := $(CONDIR)/raspimon.h $(CONDIR)/console_utils.h $(CONDIR)/pch.h $(LIB_HEADERS)
GUI_HEADERS := $(GUIDIR)/raspimon_gui.h $(GUIDIR)/gui_utils.h $(GUIDIR)/pch.h $(LIB_HEADERS)

# GTK3 flags for the GUI frontend only, straight from pkg-config
GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS   := $(shell pkg-config --libs gtk+-3.0)

# SHARED_LIBRASPIMON=1 builds and links libraspimon.so instead of the
# static archive; the $ORIGIN rpath means "next to the binary", so a
# shipped frontend + .so pair runs from any directory. Library objects
# are always compiled -fPIC so one object set serves both flavors
ifeq ($(SHARED_LIBRASPIMON),1)
  LIBRASPIMON   := libraspimon.so
  RASPIMON_LINK := -L. -lraspimon -Wl,-rpath,'$$ORIGIN'
else
  LIBRASPIMON    := libraspimon.a
  RASPIMON_LINK  := libraspimon.a
  # Only the console build can be fully static, like the old single-file
  # raspimon was
  CONSOLE_STATIC := -static
endif

all: raspimon raspimon-gui

raspimon: $(CON_OBJECTS) $(LIBRASPIMON)
	$(CXX) $(LDFLAGS) $(CONSOLE_STATIC) -o $@ $(CON_OBJECTS) $(RASPIMON_LINK) -lm

raspimon-gui: $(GUI_OBJECTS) $(LIBRASPIMON)
	$(CXX) $(LDFLAGS) -o $@ $(GUI_OBJECTS) $(RASPIMON_LINK) $(GTK_LIBS) -lm

libraspimon.a: $(LIB_OBJECTS)
	$(AR) rcs $@ $^

libraspimon.so: $(LIB_OBJECTS)
	$(CXX) -shared -o $@ $^

# Pattern rules: each foo.o is built from its matching .cc; `$<` is the
# first prerequisite, i.e. the one .cc file the % stem selected. Three
# rules because the three directories hold distinct file names
%.o: $(LIBDIR)/%.cc $(LIB_HEADERS)
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(CXXFLAGS) -fPIC -c -o $@ $<

%.o: $(CONDIR)/%.cc $(CON_HEADERS)
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(CXXFLAGS) -I$(CONDIR) -c -o $@ $<

%.o: $(GUIDIR)/%.cc $(GUI_HEADERS)
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(CXXFLAGS) -I$(GUIDIR) $(GTK_CFLAGS) -c -o $@ $<

clean:
	$(RM) raspimon raspimon-gui libraspimon.a libraspimon.so \
	  $(LIB_OBJECTS) $(CON_OBJECTS) $(GUI_OBJECTS)

.PHONY: all clean
