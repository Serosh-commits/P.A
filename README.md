# Process Analyzer

A comprehensive process monitoring tool that detects and manages zombie and orphan processes on Linux systems.

## Features

- **Real-time process monitoring** with detailed system information
- **Zombie process detection** - Processes in 'Z' state (highlighted in yellow)
- **Orphan process detection** - Processes with PPID=1 (except init, highlighted in blue)
- **Interactive process management** - Kill processes with confirmation
- **Multiple view modes** - Tree view and list view
- **Advanced filtering** - Filter by PID, PPID, state, command, CPU, memory, age
- **Sorting options** - Sort by CPU, memory, I/O, network usage
- **Process logging** - Export process data to CSV
- **Color-coded display**:
  - Green: Normal processes
  - Red: High CPU usage processes (>50%)
  - Yellow: Zombie processes
  - Blue: Orphan processes

## Compilation

```bash
make
```

Requirements:
- g++ with C++11 support
- ncurses development library (`sudo apt-get install libncurses5-dev` on Ubuntu/Debian)

## Usage

```bash
./process_analyzer
```

### Key Controls

- **Arrow Keys**: Navigate through process list
- **Page Up/Down**: Navigate by pages
- **Home/End**: Jump to first/last process
- **F4**: Apply filters (e.g., `pid:1234`, `cpu>50`, `state:Z`)
- **F5**: Toggle between tree view and list view
- **F6**: Cycle through sorting options (CPU → Memory → I/O → Network)
- **F9**: Kill selected process (with confirmation)
- **Z**: Filter to show only zombie and orphan processes
- **L**: Toggle process logging to CSV file
- **Q**: Quit the application

### Zombie and Orphan Process Management

1. **Automatic Detection**: 
   - Zombie processes (state 'Z') are highlighted in yellow
   - Orphan processes (PPID=1, except init) are highlighted in blue

2. **Quick Filter**: Press 'Z' to show only zombies and orphans

3. **Kill Processes**:
   - Select a process with arrow keys
   - Press F9 to kill
   - For zombie processes, the tool will offer to kill the parent process
   - For orphan processes, it will kill the process directly

### Filter Examples

- `pid:1234` - Show process with PID 1234
- `cpu>50` - Show processes using more than 50% CPU
- `mem>10` - Show processes using more than 10% memory
- `state:Z` - Show zombie processes
- `cmd:firefox` - Show processes with "firefox" in command name
- `age>24` - Show processes older than 24 hours

Multiple filters can be combined: `cpu>50 mem>5`

### Process Information Displayed

- **PID**: Process ID
- **PPID**: Parent Process ID
- **S**: Process State (R=Running, S=Sleeping, Z=Zombie, etc.)
- **Mem%**: Memory usage percentage
- **CPU%**: CPU usage percentage
- **IO R/W**: I/O read/write rates (KB/s)
- **RChar/WChar**: Character I/O (KB)
- **Shared/Priv**: Shared and private memory (KB)
- **FD**: File descriptor count
- **Thrd**: Thread count
- **CtxtSw**: Context switches
- **Age**: Process age in hours
- **Pri/Nice**: Priority and nice values
- **CPUs**: CPU affinity
- **Net R/W**: Network I/O rates (KB/s)
- **Cmd**: Command name

## Testing

Run the test script to create sample zombie and orphan processes:

```bash
./test_processes.sh
```

Then run the process analyzer and press 'Z' to see the test processes.

## Installation

```bash
make install
```

This will copy the executable to `/usr/local/bin/process_analyzer`.

## Troubleshooting

1. **Permission denied errors**: Run with appropriate permissions or as root for full process access
2. **Missing ncurses**: Install development libraries (`libncurses5-dev`)
3. **Compilation errors**: Ensure g++ supports C++11 (`-std=c++11`)

## Files Created

- `process_log.csv`: Process monitoring log (when logging is enabled)

## License

This tool is provided as-is for educational and system administration purposes.