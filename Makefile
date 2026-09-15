# Native build of the wind clock test suite.
#
#   make test    build and run everything
#   make table   print the dial as a calibration table
#   make clean   throw away the build directory
#
# Needs nothing but g++ (or clang++) and make. The firmware itself is built
# with PlatformIO; see platformio.ini.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror
INCLUDES  = -Isrc -Itest

BUILD_DIR = build
BINARY    = $(BUILD_DIR)/windclock_tests
TABLE_BIN = $(BUILD_DIR)/dial_table

CORE_SOURCES = $(wildcard src/core/*.cpp)
TEST_SOURCES = $(wildcard test/*.cpp)
SOURCES      = $(CORE_SOURCES) $(TEST_SOURCES)
OBJECTS      = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
CORE_OBJECTS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(CORE_SOURCES))

.PHONY: all test table clean

all: $(BINARY)

test: $(BINARY)
	@./$(BINARY)

table: $(TABLE_BIN)
	@./$(TABLE_BIN)

$(TABLE_BIN): $(CORE_OBJECTS) $(BUILD_DIR)/tools/dial_table.o
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BINARY): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
