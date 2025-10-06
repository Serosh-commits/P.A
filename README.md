# Process Analyzer

A powerful terminal-based process monitoring tool for Linux that helps detect and manage zombie and orphan processes with an interactive ncurses interface.

## Features

- **Real-time Process Monitoring**: Track CPU, memory, I/O, and network usage
- **Zombie & Orphan Detection**: Automatically highlight and filter problematic processes
- **Interactive Kill Management**: Safely terminate processes with confirmation
- **Tree and List Views**: View process hierarchy or flat list
- **Advanced Filtering**: Filter by PID, PPID, state, command, CPU, memory, and age
- **Multiple Sort Options**: Sort by CPU, memory, I/O, or network usage
- **Process Logging**: Export process data to CSV for analysis
- **Color-Coded Display**:
  - **Green**: Normal processes
  - **Red**: High CPU usage (>50%)
  - **Yellow**: Zombie processes (state 'Z')
  - **Blue**: Orphan processes (PPID=1, except init)

## Building

```bash
make
```

## Running

```bash
sudo ./process_analyzer
```

Note: Running with `sudo` is recommended to access all process information, especially I/O statistics.

## Key Bindings

### Navigation
- **Arrow Up/Down**: Move selection up/down
- **Page Up/Down**: Scroll by page
- **Home/End**: Jump to first/last process

### Features
- **F4**: Filter processes (e.g., `pid:1234 cpu>50 state:Z`)
- **F5**: Toggle between Tree and List view
- **F6**: Cycle sort criteria (CPU → Mem → I/O → Net)
- **F9**: Kill selected process (asks for confirmation)
- **Z**: Show only zombies and orphans
- **L**: Toggle CSV logging to `process_log.csv`
- **Q**: Quit

## Filter Syntax

Filters can be combined with spaces. Format: `key:value`, `key>value`, or `key<value`

Examples:
- `pid:1234` - Show only process with PID 1234
- `cpu>50` - Show processes using more than 50% CPU
- `mem>10` - Show processes using more than 10% memory
- `state:Z` - Show only zombie processes
- `cmd:chrome` - Show processes with 'chrome' in command name
- `age>1` - Show processes older than 1 hour
- `cpu>50 mem>10` - Combine multiple filters (AND logic)

## Zombie & Orphan Process Management

### What are Zombie Processes?
Zombie processes (state 'Z') are terminated processes whose parent hasn't read their exit status. They consume minimal resources but indicate potential issues.

### What are Orphan Processes?
Orphan processes have a parent PID of 1 (init/systemd), typically because their original parent died.

### How to Use

1. **Quick Check**: Press `Z` to filter and show only zombies and orphans
2. **Review**: The tool will display count of found processes
3. **Kill**: Select a process and press `F9`
   - For **zombies**: The tool automatically offers to kill the parent process (which usually cleans up zombies)
   - For **orphans**: Offers to kill the orphan directly
4. **Confirm**: Press `y` to confirm or `n` to cancel

If no zombies or orphans are found, the display will show "No processes to display".

## Display Columns

| Column | Description |
|--------|-------------|
| PID | Process ID |
| PPID | Parent Process ID |
| S | State (R=Running, S=Sleeping, Z=Zombie, etc.) |
| Mem% | Memory usage percentage |
| CPU% | CPU usage percentage |
| IO R | I/O read rate (KB/s) |
| IO W | I/O write rate (KB/s) |
| RChar | Total characters read (KB) |
| WChar | Total characters written (KB) |
| Shared | Shared clean memory (KB) |
| Priv | Private dirty memory (KB) |
| FD | File descriptor count |
| Thrd | Thread count |
| CtxtSw | Voluntary context switches |
| Age | Process age (hours) |
| Pri | Priority |
| Nice | Nice value |
| CPUs | Allowed CPU list |
| Net R | Network receive rate (KB/s) |
| Net W | Network transmit rate (KB/s) |
| Cmd | Command name |

## Logging

When logging is enabled (press `L`), all process information is continuously written to `process_log.csv` in the current directory. The CSV includes timestamps for time-series analysis.

## System Requirements

- Linux kernel with `/proc` filesystem
- ncurses library (`libncurses-dev`)
- C++11 compatible compiler

## Troubleshooting

**Can't see I/O statistics**: Run with `sudo` for full I/O access

**Missing processes**: Some processes may be inaccessible without root privileges

**Display issues**: Ensure terminal supports color and is large enough (minimum 120 columns recommended)

## Example Workflow: Finding and Cleaning Zombies

```
1. Run: sudo ./process_analyzer
2. Press 'Z' to filter zombies/orphans
3. Check status message: "Filtered zombies and orphans (X found)"
4. If X > 0:
   - Navigate to zombie process
   - Press F9
   - Confirm to kill parent process
5. If X = 0:
   - Your system is clean!
```

## License

MIT License - Feel free to modify and distribute
