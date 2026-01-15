# Process Analyzer

Process Analyzer is a lightweight, efficient process monitor designed for Linux systems. It provides a terminal-based interface using Ncurses to monitor system health and navigate the process tree with minimal overhead.

This tool was developed as a high-performance alternative to standard process viewers, focusing on providing deep visibility into process states (including specialized detection for zombies and orphans) while maintaining a low resource footprint.

## Architecture

The project is built with a modular C++11 architecture, separated into distinct engines to ensure the codebase remains maintainable and extensible:

- **SystemUtils**: Responsible for high-frequency scraping of the `/proc` filesystem and calculating deltas for CPU, Memory, IO, and Network usage.
- **FilterEngine**: A custom query engine that supports complex filters like `cpu>50`, `state:Z`, or `cmd:python`.
- **DisplayEngine**: Manages the Ncurses rendering pipeline, supporting both hierarchical tree views and flattened lists.
- **ProcessSorter**: Provides efficient sorting across multiple metrics including disk IO and network throughput.
- **ProcessLogger**: Handles background logging of process snapshots to CSV for long-term analysis.

## Core Features

- **Low Overhead**: Consistently uses minimal memory and CPU cycles, making it suitable for monitoring heavily loaded production servers.
- **Comprehensive Metrics**: Displays CPU and memory usage, IO read/write rates, network RX/TX rates, file descriptor counts, thread counts, and context switch frequency.
- **Zombie & Orphan Management**: Dedicated logic to identify defunct processes and orphans, with integrated tools to send termination signals (SIGTERM) to parents or the processes themselves.
- **Advanced Filtering**: Live filtering using a simple key-value syntax to isolate specific workloads.
- **CSV Logging**: Toggleable logging to `process_log.csv` for post-incident investigations.

## Installation and Build

The project requires `g++` and the `ncurses` development library.

1. Clone the repository:
   ```bash
   git clone https://github.com/Serosh-commits/P.A.git
   cd P.A
   ```

2. Build the project using the provided Makefile:
   ```bash
   make
   ```

3. Run the analyzer:
   ```bash
   ./pa
   ```

To output a single snapshot in JSON format (useful for script integration):
```bash
./pa --json
```

## Interface Controls

- **F4**: Apply a filter (e.g., `cmd:nginx`, `cpu>10`).
- **F5**: Toggle between Tree view and Flat List view.
- **F6**: Cycle through sorting criteria (CPU, Memory, IO, Network).
- **F9**: Send a SIGTERM signal to the selected process.
- **Z**: Filter for zombies and orphaned processes only.
- **L**: Toggle CSV logging to disk.
- **Q**: Quit the application.

## Design Philosophy

The primary objective of Process Analyzer is to provide immediate, actionable intelligence about system processes without the complexity or weight of larger monitoring suites. By interacting directly with kernel-exposed data in `/proc`, it offers a transparent view of system behavior.
