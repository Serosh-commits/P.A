🚀 Process Analyzer
   
A lightweight, htop-like process monitoring tool for Linux, written in C++ with an interactive ncurses-based UI. It displays detailed process metrics, supports advanced filtering, sorting, tree/list views, and allows killing zombie and orphan processes directly from the UI. Perfect for system administrators and developers debugging resource usage.
✨ Features

🔍 Advanced Filtering: Filter by PID, PPID, state (e.g., state:Z for zombies), CPU, memory, or age with AND logic (e.g., cmd:bash cpu>50).

📊 Real-Time Metrics: PID, PPID, state, memory/CPU %, I/O/network rates, file descriptors, threads, context switches, process age, priority, nice, and CPU affinity.

🛡️ Zombie/Orphan Handling: Detects and kills zombies (by parent) and orphans via F9.

📝 Logging: Export data to process_log.csv with timestamped metrics.

📋 Interactive Controls:



Key
Action



↑/↓
Navigate process list


PgUp/PgDn
Scroll page-wise


Home/End
Jump to top/bottom


F4
Filter (e.g., pid:1234 cpu>50)


F5
Toggle tree/list view


F6
Cycle sort (CPU, Mem, IO, Net)


F9
Kill selected process


L
Toggle logging


Q
Quit



📈 System Stats: Header shows system CPU, memory, uptime, and cores.

🔄 Responsive Design: Handles terminal resizing, edge cases, and low CPU usage (1s updates).


📋 Requirements

OS: Linux (relies on /proc filesystem).
Compiler: g++ with C++17 support.
Library: ncurses (install: sudo apt-get install libncurses5-dev libncursesw5-dev on Debian/Ubuntu).
Permissions: Run with sudo for full /proc access.

🛠️ Installation

Clone the repo:
git clone https://github.com/Serosh-commits/P.A.git
cd process-analyzer


Compile:
g++ -std=c++17 -o analyzer pa.cpp -lncurses



🚀 Usage

Run with sudo:
sudo ./analyzer


Quick Start:

The UI launches immediately with a process list.
Use ↑/↓ to select a process.
Press F9 to kill (zombies/orphans handled specially).


Advanced Examples:

Filter Zombies: Press F4, enter state:Z, Enter. Select and F9 to kill parent.
Filter Bash Processes >10% CPU: F4, enter cmd:bash cpu>10, Enter.
Sort by Network: F6 until "Sort: net" in header.
Toggle Logging: L to start CSV export, check process_log.csv.


Create Test Cases:

Zombie: sleep 100 &
kill -STOP $(pidof sleep)

Filter state:Z, select, F9.
Orphan:bash -c 'sleep 100 &'
kill $(pidof bash)

Filter ppid:1, select, F9.



📊 Output
UI Preview
System CPU: 5.23% Mem: 42.1% Uptime: 2.45 h | Cores: 8 | Logging: OFF | Sort: None | Filter: None
F4: Filter (e.g., pid:1234 cpu>50) | F5: Tree/List | F6: Sort | F9: Kill | L: Log | Q: Quit
PID   PPID  S    Mem%  CPU%  IO R  IO W  RChar WChar Shared Priv  FD   Thrd CtxtSw Age  Pri  Nice CPUs  Net R Net W Cmd                  
1234  1     R    1.23  0.45  0.00 0.00  123   456   789   101   2    4    1000  0.50 20   0    0-3   0.00 0.00 bash                  
5678  1234  S    0.56  12.34 1.23 2.45  678   901   234   567   5    8    2000  1.20 19   -5   0     10.5 5.67 python3               

Log File (process_log.csv)
Timestamp,PID,PPID,State,Cmd,Mem%,CPU%,IO R (KB/s),IO W (KB/s),RChar (KB),WChar (KB),Shared (KB),Private (KB),FD,Threads,CtxtSw,Age (h),Priority,Nice,CPUs,Net R (KB/s),Net W (KB/s)
Wed Oct  1 21:13:45 2025,1234,1,R,bash,1.23,0.45,0.00,0.00,123,456,789,101,2,4,1000,0.50,20,0,0-3,0.00,0.00,bash

⚠️ Notes

Permissions: sudo is required for /proc/pid/io and other files.
Performance: 1s updates, low CPU (efficient redraws, 10ms polling).
Error Handling: Status messages show issues (e.g., "Invalid filter: cpu:abc").
Dependencies: Ncurses; install if missing.
Zombie/Orphan: Zombies ('Z' state), orphans (PPID=1, PID!=1).

🤝 Contributing

Fork the repo.
Create a feature branch (git checkout -b feature/amazing-feature).
Commit changes (git commit -m 'Add amazing feature').
Push to branch (git push origin feature/amazing-feature).
Open a Pull Request.

⭐ Star this repo if it helps you! 🚀
