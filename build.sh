#!/bin/bash

set -e

echo "Building LRU Cache Interview Project..."

# Clean previous build
rm -f demo

# Compile the demo
echo "Compiling demo..."
g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 -I./include -pthread src/main.cpp -o demo

echo "Build completed successfully!"
echo "Run './demo' to execute the demonstration program."

# Try to compile tests if Google Test is available
if pkg-config --exists gtest; then
    echo "Google Test found, compiling tests..."
    g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 -I./include -pthread \
        tests/test_lru_cache.cpp tests/test_thread_safety.cpp \
        $(pkg-config --cflags --libs gtest) -o tests
    echo "Tests compiled successfully! Run './tests' to execute them."
else
    echo "Google Test not found. Tests not compiled."
    echo "Install Google Test to compile and run unit tests:"
    echo "  macOS: brew install googletest"
    echo "  Ubuntu: sudo apt install libgtest-dev"
fi