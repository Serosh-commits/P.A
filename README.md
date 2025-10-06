# Process Analyzer - Zombie and Orphan Process Detector

A comprehensive process monitoring tool that detects and manages zombie and orphan processes on Linux systems.

## Features

- **Zombie Process Detection**: Identifies processes in zombie state (state 'Z')
- **Orphan Process Detection**: Identifies processes with parent PID=1 that are not init
- **Interactive Process Management**: Allows killing of problematic processes
- **Real-time Monitoring**: Live process statistics with configurable refresh rate
- **Advanced Filtering**: Filter processes by various criteria including zombie/orphan status
- **Tree/List Views**: Switch between hierarchical and flat process views
- **Comprehensive Logging**: CSV logging of process data including zombie/orphan flags
- **Color-coded Display**: Visual indicators for different process types

## Installation

1. Install dependencies:
```bash
make install-deps
```

2. Compile the program:
```bash
make
```

## Usage

Run the process analyzer:
```bash
./process_analyzer
```

### Controls

- **Arrow Keys**: Navigate through processes
- **Page Up/Down**: Jump through pages
- **Home/End**: Go to first/last process
- **F4**: Apply filters (e.g., `zombie:true`, `orphan:true`, `cpu>50`)
- **F5**: Toggle between tree and list view
- **F6**: Cycle through sort options (cpu, mem, io, net, zombie, orphan)
- **F9**: Kill selected process (with confirmation)
- **Z**: Filter to show only zombies and orphans
- **L**: Toggle logging on/off
- **Q**: Quit

### Filter Examples

- `zombie:true` - Show only zombie processes
- `orphan:true` - Show only orphan processes
- `cpu>50` - Show processes using more than 50% CPU
- `mem>10` - Show processes using more than 10% memory
- `pid:1234` - Show specific process by PID
- `cmd:firefox` - Show processes with "firefox" in command name

### Process Types

- **Zombie Processes**: Dead processes that haven't been cleaned up by their parent
- **Orphan Processes**: Processes whose parent has died and been adopted by init (PID 1)

### Killing Processes

When you select a process and press F9:
- **Zombie Process**: Offers to kill the parent process (which should clean up the zombie)
- **Orphan Process**: Offers to kill the orphan process directly
- **Normal Process**: Offers to kill the process directly

## Output Columns

- **PID**: Process ID
- **PPID**: Parent Process ID
- **S**: Process state (R=Running, S=Sleeping, Z=Zombie, etc.)
- **Mem%**: Memory usage percentage
- **CPU%**: CPU usage percentage
- **IO R/W**: I/O read/write rates (KB/s)
- **RChar/WChar**: Characters read/written (KB)
- **Shared/Priv**: Shared/Private memory (KB)
- **FD**: File descriptor count
- **Thrd**: Thread count
- **CtxtSw**: Context switches
- **Age**: Process age in hours
- **Pri/Nice**: Priority and nice values
- **CPUs**: CPU affinity
- **Net R/W**: Network receive/transmit rates (KB/s)
- **Cmd**: Command name
- **Z**: Zombie indicator
- **O**: Orphan indicator

## Logging

When logging is enabled (press L), the tool creates a `process_log.csv` file with detailed process information including zombie and orphan flags.

## Technical Details

- Uses `/proc` filesystem for process information
- Real-time CPU and memory usage calculation
- Network and I/O statistics monitoring
- Color-coded display for easy identification of problematic processes
- Efficient process tree building and traversal

## Requirements

- Linux system with `/proc` filesystem
- C++17 compatible compiler
- ncurses library
- Root privileges recommended for full functionality

## Troubleshooting

- If you can't see all processes, run with `sudo`
- For zombie processes, killing the parent usually resolves the issue
- Orphan processes can typically be killed directly
- Use the Z key to quickly filter to problematic processes