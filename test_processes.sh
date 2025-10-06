#!/bin/bash

# Script to create test processes for the process analyzer

echo "Creating test processes..."

# Create a zombie process
create_zombie() {
    if [ $$ -ne 1 ]; then
        # Fork a child process that exits immediately
        (
            sleep 0.1
            exit 0
        ) &
        # Parent sleeps to keep zombie alive
        sleep 30 &
        ZOMBIE_PARENT_PID=$!
        echo "Created zombie process (parent PID: $ZOMBIE_PARENT_PID)"
    fi
}

# Create an orphan process
create_orphan() {
    # Create a process that will become orphaned
    (
        # This process will become orphaned when its parent exits
        sleep 60 &
        ORPHAN_PID=$!
        echo "Created potential orphan process (PID: $ORPHAN_PID)"
        exit 0
    ) &
    # Wait a bit for the child to start, then exit to orphan it
    sleep 1
}

echo "Creating zombie process..."
create_zombie

echo "Creating orphan process..."
create_orphan

echo "Test processes created. You can now run the process analyzer:"
echo "./process_analyzer"
echo ""
echo "Use 'Z' key to filter for zombies and orphans"
echo "Use F9 to kill selected processes"
echo "Use 'Q' to quit"

# Keep this script running for a bit
sleep 5
echo "Test script completed."