# Simple Makefile in src: build solver directly in this folder

CXX      ?= g++
CXXFLAGS ?= -std=c++11 -Wall -g -Ofast
# Suppress compiler warnings during build; override WARNFLAGS to change behavior
WARNFLAGS ?= -w
INCLUDES ?= -I.
LDFLAGS  ?=

TARGET := solver
SRCS   := $(wildcard *.cpp)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(WARNFLAGS) $(INCLUDES) $(SRCS) -o $@ $(LDFLAGS)

clean:
	$(RM) $(TARGET)
