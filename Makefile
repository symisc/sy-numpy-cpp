PREFIX ?= /usr/local
DESTDIR ?=

CXX ?= g++
AR ?= ar
RANLIB ?= ranlib

BUILD_DIR ?= build
OBJ_DIR := $(BUILD_DIR)/obj

CPPFLAGS ?= -I.
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS ?=

LIB_NAME := libsynumpy.a
LIB_TARGET := $(BUILD_DIR)/$(LIB_NAME)
EXAMPLE_TARGET := $(BUILD_DIR)/example

LIB_OBJ := $(OBJ_DIR)/synumpy.o
EXAMPLE_OBJ := $(OBJ_DIR)/example.o

.PHONY: all lib example clean install uninstall

all: lib example

lib: $(LIB_TARGET)

example: $(EXAMPLE_TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/synumpy.o: synumpy.cpp synumpy.hpp | $(OBJ_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/example.o: example.cpp synumpy.hpp | $(OBJ_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(LIB_TARGET): $(LIB_OBJ) | $(BUILD_DIR)
	$(AR) rcs $@ $^
	$(RANLIB) $@

$(EXAMPLE_TARGET): $(EXAMPLE_OBJ) $(LIB_TARGET) | $(BUILD_DIR)
	$(CXX) $(LDFLAGS) -o $@ $(EXAMPLE_OBJ) $(LIB_TARGET) $(LDLIBS)

install: $(LIB_TARGET)
	install -d $(DESTDIR)$(PREFIX)/include
	install -d $(DESTDIR)$(PREFIX)/lib
	install -m 644 synumpy.hpp $(DESTDIR)$(PREFIX)/include/synumpy.hpp
	install -m 644 $(LIB_TARGET) $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/include/synumpy.hpp
	rm -f $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME)

clean:
	rm -rf $(BUILD_DIR)
