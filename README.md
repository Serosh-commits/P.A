# Process Analyzer

A comprehensive process monitoring and management tool that detects and handles zombie and orphan processes.

## Features

- **Real-time Process Monitoring**: Displays all running processes with detailed information
- **Zombie Process Detection**: Automatically detects zombie processes (state 'Z')
- **Orphan Process Detection**: Identifies orphan processes (parent PID = 1, but not init)
- **Interactive Management**: Kill processes with confirmation prompts
- **Tree/List View**: Switch between hierarchical tree view and flat list view
- **Filtering**: Filter processes by various criteria (PID, CPU usage, memory usage, etc.)
- **Sorting**: Sort processes by CPU, memory, I/O, or network usage
- **Logging**: Export process data to CSV format
- **Color-coded Display**: Different colors for zombies, orphans, and high CPU processes

## Compilation

```bash
make
```

## Usage

```bash
./process_analyzer
```

## Controls

- **Arrow Keys**: Navigate through processes
- **Page Up/Down**: Scroll through process list
- **Home/End**: Jump to first/last process
- **F4**: Apply filters (e.g., `pid:1234`, `cpu>50`, `mem<10`)
- **F5**: Toggle between tree view and list view
- **F6**: Cycle through sorting options (CPU, Memory, I/O, Network)
- **F9**: Kill selected process (with confirmation)
- **Z**: Detect and handle zombies/orphans automatically
- **L**: Toggle logging to CSV file
- **Q**: Quit the application

## Zombie and Orphan Detection

The tool automatically detects:
- **Zombie Processes**: Dead processes waiting for their parent to reap them (state 'Z')
- **Orphan Processes**: Processes whose parent has died (PPID = 1, but PID ≠ 1)

When zombies or orphans are detected:
1. The tool displays a list of problematic processes
2. Asks for confirmation before killing them
3. For zombies: Kills the parent process to clean up the zombie
4. For orphans: Kills the orphan process directly

## Process Information Displayed

- **PID**: Process ID
- **PPID**: Parent Process ID
- **S**: Process state (R=Running, S=Sleeping, D=Disk sleep, Z=Zombie, etc.)
- **Mem%**: Memory usage percentage
- **CPU%**: CPU usage percentage
- **IO R/W**: I/O read/write rates (KB/s)
- **RChar/WChar**: Characters read/written
- **Shared/Priv**: Shared and private memory (KB)
- **FD**: File descriptor count
- **Thrd**: Thread count
- **CtxtSw**: Context switches
- **Age**: Process age in hours
- **Pri/Nice**: Priority and nice values
- **CPUs**: CPU affinity
- **Net R/W**: Network receive/transmit rates (KB/s)
- **Cmd**: Command name

## Color Coding

- **Green**: Normal processes
- **Red**: High CPU usage processes (>50%)
- **Yellow**: Zombie processes
- **Blue**: Orphan processes

## Requirements

- Linux system with `/proc` filesystem
- ncurses library
- C++11 compatible compiler

## Installation

```bash
make install
```

This installs the binary to `/usr/local/bin/process_analyzer`.

## Uninstallation

```bash
make uninstall
```

## Logging

When logging is enabled (press 'L'), the tool creates a `process_log.csv` file with detailed process information that can be imported into spreadsheet applications for analysis.