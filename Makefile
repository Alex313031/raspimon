# Copyright (c) 2026 Alex313031

# raspimon Makefile for gcc

SRCDIR  := src
TARGET  := raspimon

ifeq ($(IS_DEBUG),1)
  CPPFLAGS += -DDEBUG -D_DEBUG
  CFLAGS   += -Wall -Og -g2
  CXXFLAGS += -Wall -Og -g2 -std=c++17
  LDFLAGS  += -static
else
  CPPFLAGS += -DNDEBUG -D_NDEBUG
  CFLAGS   += -Wno-error -O2 -g0
  CXXFLAGS += -Wno-error -O2 -g0 -std=c++17
  LDFLAGS  += -static -s
endif

OBJECTS := raspimon.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) -lm

$(OBJECTS): $(SRCDIR)/raspimon.cc $(SRCDIR)/raspimon.h
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	$(RM) $(TARGET) $(OBJECTS)

.PHONY: all clean
