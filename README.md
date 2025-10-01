Process Analyzer
A lightweight, htop-like process monitoring tool for Linux, written in C++ with an interactive ncurses-based interface. It displays detailed process metrics, supports filtering, sorting, tree/list views, and allows killing zombie and orphan processes directly from the UI.
Features

Interactive UI: Ncurses-based interface with real-time process monitoring, similar to htop.
Process Metrics: Displays PID, PPID, state, memory %, CPU %, I/O rates, network rates, file descriptors, threads, context switches, process age, priority, nice, and CPU affinity.
Zombie/Orphan Handling: Detects and allows killing zombie processes (by parent) and orphan processes via the F9 key.
Keybindings:
Up/Down: Navigate process list.
PageUp/PageDown: Scroll page-wise.
Home/End: Jump to top/bottom.
F4: Filter processes by command name.
F5: Toggle between tree and list views.
F6: Cycle sort by CPU, memory, I/O, or network usage.
F9: Kill selected process (zombie: parent, orphan: process, regular: process).
L: Toggle CSV logging.
Q: Quit.

Sorting: Sort by CPU, memory, I/O, or network usage (F6).
Filtering: Filter processes by command name (F4).
Logging: Logs process data to process_log.csv (toggle with L).
Visual Feedback: Status messages for actions (e.g., "SIGTERM sent", "Filter applied").
Color Coding: Green (normal), red (high CPU >50%), yellow (zombies), blue (orphans).
System Stats: Shows system CPU usage, memory usage, uptime, and core count in the header.
Responsive Design: Handles terminal resizing and edge cases (e.g., empty process list).

Requirements

OS: Linux (uses /proc filesystem).
Compiler: g++ with C++17 support.
Library: ncurses (install via sudo apt-get install libncurses5-dev libncursesw5-dev on Debian/Ubuntu).
Permissions: Run with sudo for full /proc access (e.g., /proc/pid/io).

Installation

Clone the repository:
git clone https://github.com/yourusername/process-analyzer.git
cd process-analyzer


Compile the program:
g++ -std=c++17 -o analyzer process_analyzer.cpp -lncurses



Usage

Run the program with sudo:
sudo ./analyzer


Keybindings:

Navigate: Use Up/Down, PageUp/PageDown, Home/End to move through the process list.
Filter: Press F4, enter a command name (e.g., "bash"), press Enter. Clear filter with empty input.
Toggle View: Press F5 to switch between tree (hierarchical) and list views.
Sort: Press F6 to cycle sort modes (CPU, Mem, IO, Net).
Kill Process: Select a process, press F9:
Zombie: Prompts to kill parent (PPID).
Orphan: Prompts to kill process.
Regular: Prompts to kill process.
Press 'y' to confirm, any key to cancel.


Log: Press L to toggle CSV logging to process_log.csv.
Quit: Press Q to exit.


Output:

The UI shows a header with system CPU, memory, uptime, cores, and logging status.
Process list includes PID, PPID, state, and various metrics.
Status messages (in red) show actions or errors (e.g., "SIGTERM sent", "Filter applied: bash").
Log file (process_log.csv) contains timestamped process data when logging is enabled.



Example

Start the tool:
sudo ./analyzer


Test Zombie Process:

In another terminal, create a zombie:sleep 100 &
kill -STOP $(pidof sleep)


In the UI, find the process (yellow, state 'Z'), select it, press F9, confirm with 'y' to kill parent.


Test Orphan Process:

Run:bash -c 'sleep 100 &'
kill $(pidof bash)


In the UI, find the process (blue, PPID=1), select it, press F9, confirm with 'y' to kill.


Check Logs:

Enable logging (L), check process_log.csv for process data.



Output Format

UI Columns:
PID, PPID, S (state), Mem%, CPU%, IO R/W (KB/s), RChar/WChar (KB), Shared/Priv (KB), FD, Thrd, CtxtSw, Age (h), Pri, Nice, CPUs, Net R/W (KB/s), Cmd.


Log File (process_log.csv):Timestamp,PID,PPID,State,Cmd,Mem%,CPU%,IO R (KB/s),IO W (KB/s),RChar (KB),WChar (KB),Shared (KB),Private (KB),FD,Threads,CtxtSw,Age (h),Priority,Nice,CPUs,Net R (KB/s),Net W (KB/s)
Wed Oct  1 21:13:45 2025,1234,1,R,bash,1.23,0.45,0.00,0.00,123,456,789,101,2,4,1000,0.50,20,0,0-3,0.00,0.00,bash

Notes

Permissions: Run with sudo to access /proc/pid/io and other restricted files.
Performance: Updates every 1 second, with low CPU usage due to efficient redraws and 10ms input polling.
Error Handling: Displays errors (e.g., "Error: Cannot open /proc") in the UI.
Dependencies: Requires ncurses; install it if not present.
Zombie/Orphan Detection: Zombies have state 'Z', orphans have PPID=1 (excluding PID=1).

Contributing
Contributions are welcome! Please submit issues or pull requests for bugs, features, or improvements. Ensure code follows C++17 standards and includes comments for clarity.
