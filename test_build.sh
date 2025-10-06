#!/bin/bash

echo "=== Process Analyzer Build Test ==="
echo

# Test 1: Check if executable exists
echo "Test 1: Checking if executable exists..."
if [ -f "./process_analyzer" ]; then
    echo "✓ Executable found"
else
    echo "✗ Executable not found"
    exit 1
fi

# Test 2: Check if executable is runnable
echo
echo "Test 2: Checking if executable is runnable..."
if [ -x "./process_analyzer" ]; then
    echo "✓ Executable has execute permissions"
else
    echo "✗ Executable lacks execute permissions"
    exit 1
fi

# Test 3: Check for required libraries
echo
echo "Test 3: Checking for required libraries..."
if ldd ./process_analyzer | grep -q "libncurses"; then
    echo "✓ ncurses library linked"
else
    echo "✗ ncurses library not linked"
    exit 1
fi

# Test 4: Verify /proc is accessible
echo
echo "Test 4: Checking /proc filesystem..."
if [ -d "/proc" ] && [ -r "/proc/stat" ]; then
    echo "✓ /proc filesystem accessible"
else
    echo "✗ /proc filesystem not accessible"
    exit 1
fi

# Test 5: Count current processes
echo
echo "Test 5: Checking system processes..."
PROC_COUNT=$(ls -d /proc/[0-9]* 2>/dev/null | wc -l)
echo "✓ Found $PROC_COUNT processes in /proc"

echo
echo "=== All Tests Passed ==="
echo
echo "To run the process analyzer:"
echo "  sudo ./process_analyzer"
echo
echo "Key features:"
echo "  - Press 'Z' to filter zombies and orphans"
echo "  - Press 'F9' to kill selected process"
echo "  - Press 'Q' to quit"
