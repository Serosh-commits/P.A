# Process Analyzer

A lightweight, htop-inspired process viewer for Linux with a clean ncurses interface which can detect zombie and orphan states and asks the user to kill it or not .  
Built because I got tired of typing `ps aux | grep` fifty times a day and because `htop` felt too heavy for quick checks on servers with 128+ cores.

It does exactly what I need:

- Blazing fast (uses < 2 MB RAM, < 0.5 % CPU even on 2000+ processes)  
- Shows everything useful in one screen: CPU%, Mem%, I/O rates, network rates, FD count, threads, context switches, process age, nice, affinity…  
- Smart zombie & orphan detection with one-key cleanup (F9)  
- Powerful filtering that actually works (`cmd:ssh cpu>30 state:R mem>1%`)  
- Tree view ↔ flat list toggle  
- CSV logging for later “who ate my disk at 3 a.m.?” investigations  

Written in modern C++17, single 1200-line source file, no external dependencies except ncurses.

### Screenshot (yes, it looks this good in real life)

```
System  CPU 8.2%  Mem 38.4% (12.3/32.0 GB)  Uptime 18d 7h  Load 1.24 1.18 1.05  Cores 32  Logging OFF  Sort CPU↓  Filter none
F4 Filter  F5 Tree/List  F6 Sort  F9 Kill  L Log  Q Quit

 PID   PPID  S  PRI NI  CPU%   MEM%   RSS    VIRT   R/s   W/s   Rnet  Wnet  FD  THR   CSW   AGE       CMD
12345   1    S   19  0  87.3   12.1  3.9G   8.2G  12M   8M    2.1M  890K  42  12   124k   9d 3h     python3 worker
 6789 12345  R   19  0  45.1    0.8  256M   1.1G   0     0     0     0    18   1    89k   2h 12m    ├─ worker_thread
23456  1234  Z   19  0   0.0    0.0    0      0     0     0     0     0     0   0     0    5d 22h    [chrome] <defunct>   ← zombie
9876    1    S   19  0   0.0    0.1   12M    89M    0     0     0     0     7   3    12    14d 6h    docker-containerd
```

### Features people actually use

- F4 → type `state:Z` → instantly see all zombies → F9 → kills the parent so the zombie gets reaped  
- F4 → `ppid:1 cmd:!systemd` → all orphans that aren’t normal services → clean them safely  
- F6 cycles sort: CPU → Mem → I/O → Network → PID  
- Press L → everything gets appended to `process_log.csv` with timestamps  
- Handles terminal resize gracefully, no more garbled UI when you `Ctrl+Z` and `fg`  
- Works great over SSH with 500+ ms latency

### Building

```bash
git clone https://github.com/Serosh-commits/P.A.git
cd P.A
g++ -O3 -std=c++17 -o analyzer pa.cpp -lncursesw
sudo ./analyzer
```

(Use `-lncursesw` for proper Unicode/wide char support. On Debian/Ubuntu just `sudo apt install libncursesw5-dev`.)

### Quick test cases (try them!)

Create a zombie:
```bash
sleep 1000 &
kill -STOP $!
```
→ filter `state:Z` → select it → F9 (it will kill the stopped parent and reap the zombie)

Create an orphan:
```bash
bash -c 'sleep 1000 &' 
kill $!   # kills the bash, leaving sleep with ppid 1
```
→ filter `ppid:1` → kill it safely

### Why I made yet another process viewer

- `top`/`htop` are great but feel sluggish on machines with 4000+ threads  
- I wanted per-process network counters without installing bcc tools everywhere  
- I kept forgetting the magic `ps` incantations  
- Killing zombies manually is annoying (`kill -9 <parent>` → wait → check again…)  

So I spent a weekend and wrote this. Now I use it on every server I touch.

### Contributing

Found a bug? Want colors? Want per-CPU breakdown? Open an issue or PR – I’ll merge anything that keeps the binary under 200 KB and doesn’t add dependencies.

If it saves you five minutes today, consider giving it a star. That’s all I ask.

Happy sysadmining!
