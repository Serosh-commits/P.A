# P.A (Process Analyzer)

P.A is a fast, lightweight system monitor for Linux written in C++11 and ncurses. I built it as an alternative to htop, focusing on direct `/proc` parsing and better handling of process states, zombies, and kernel threads.

## Features

- **Htop-style UI**: Multi-colored bars for CPU (User/Sys/Nice/IRQ) and Memory (Used/Buffers/Cache).
- **Accurate network & IO tracking**: Properly ignores `PF_KTHREAD` (kernel threads) so they don't bleed into network and IO stats.
- **Filtering & Search**: Incremental search (`/`) and live advanced filtering (`\`) to easily isolate specific workloads. 
- **Tree & List views**: Toggle between hierarchical process trees and flat lists.
- **Process management**: Built-in support for sending signals and purging zombies directly from the UI.
- **JSON mode**: Run `./pa --json` to get a two-scan live snapshot of the system for scripting.

## Building

You just need `g++` and the ncurses development headers (`libncurses5-dev` or `ncurses-devel`).

```bash
git clone https://github.com/Serosh-commits/P.A.git
cd P.A
make
./pa
```

## Keybindings

- `F1` or `h` or `?`: Show help
- `F3` or `/`: Search for a process by name
- `F4` or `\`: Filter processes (e.g., `cpu>50`, `cmd:python`)
- `F5` or `t`: Toggle between tree and list view
- `F6` or `>` or `.`: Cycle sort column (CPU, Mem, IO, Net)
- `F9` or `k`: Kill selected process
- `F10` or `q`: Quit
- `P`: Sort by CPU
- `M`: Sort by Memory
- `N`: Sort by PID
- `I`: Invert sort order
- `z`: Show only zombies/orphans
- `x`: Purge zombies (sends standard signals to their parents)
- `l`: Toggle CSV logging
