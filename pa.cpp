#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <ncurses.h>

struct ProcessInfo {
    pid_t pid;
    pid_t ppid;
    char state;
    std::string cmd;
    long rss;
    double mem_usage;
    unsigned long utime;
    unsigned long stime;
    double cpu_usage;
    unsigned long long read_bytes;
    unsigned long long write_bytes;
    double io_read_rate;
    double io_write_rate;
    unsigned long long rchar;
    unsigned long long wchar;
    long num_threads;
    unsigned long long voluntary_ctxt_switches;
    unsigned long long shared_clean;
    unsigned long long private_dirty;
    unsigned long long fd_count;
    double process_age;
    long priority;
    long nice;
    std::string cpus_allowed_list;
    unsigned long long net_rx_bytes;
    unsigned long long net_tx_bytes;
    double net_rx_rate;
    double net_tx_rate;
};

class ProcessAnalyzer {
private:
    std::vector<ProcessInfo> processes;
    std::map<pid_t, std::vector<pid_t>> process_tree;
    std::map<pid_t, ProcessInfo> process_map;
    unsigned long long mem_total;
    int num_cores;
    std::vector<ProcessInfo> prev_processes;
    unsigned long long prev_total_jiffies;
    unsigned long long prev_work_jiffies;
    double poll_interval = 5.0;
    double system_uptime = 0.0;
    long clk_tck;
    std::ofstream log_file;
    bool logging_enabled = false;
    int selected_row = 0;
    int scroll_offset = 0;
    bool tree_view = true;
    std::string sort_criterion = "";
    std::string filter_str = "";
    WINDOW *win;

    void scanProcesses() {
        processes.clear();
        process_tree.clear();
        process_map.clear();
        DIR *proc_dir = opendir("/proc");
        if (!proc_dir) return;
        struct dirent *entry;
        while ((entry = readdir(proc_dir))) {
            if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
                pid_t pid = std::stoi(entry->d_name);
                ProcessInfo info;
                std::string path = "/proc/" + std::to_string(pid) + "/stat";
                std::ifstream stat_file(path);
                if (!stat_file) continue;
                std::string line;
                std::getline(stat_file, line);
                std::istringstream iss(line);
                int dummy;
                std::string comm;
                unsigned long long starttime;
                iss >> info.pid >> comm >> info.state >> info.ppid >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> info.utime >> info.stime >> dummy >> dummy >> info.num_threads >> dummy >> dummy >> starttime >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> info.priority >> info.nice;
                comm = comm.substr(1, comm.size() - 2);
                info.cmd = comm;
                info.process_age = (system_uptime - static_cast<double>(starttime) / clk_tck) / 3600.0;
                stat_file.close();
                path = "/proc/" + std::to_string(pid) + "/status";
                std::ifstream status_file(path);
                if (!status_file) continue;
                while (std::getline(status_file, line)) {
                    std::istringstream status_iss(line);
                    std::string key;
                    status_iss >> key;
                    if (key == "VmRSS:") status_iss >> info.rss;
                    else if (key == "voluntary_ctxt_switches:") status_iss >> info.voluntary_ctxt_switches;
                    else if (key == "Cpus_allowed_list:") status_iss >> info.cpus_allowed_list;
                }
                status_file.close();
                path = "/proc/" + std::to_string(pid) + "/io";
                std::ifstream io_file(path);
                if (io_file) {
                    while (std::getline(io_file, line)) {
                        std::istringstream io_iss(line);
                        std::string key;
                        io_iss >> key;
                        if (key == "read_bytes:") io_iss >> info.read_bytes;
                        else if (key == "write_bytes:") io_iss >> info.write_bytes;
                        else if (key == "rchar:") io_iss >> info.rchar;
                        else if (key == "wchar:") io_iss >> info.wchar;
                    }
                    io_file.close();
                } else {
                    info.read_bytes = 0;
                    info.write_bytes = 0;
                    info.rchar = 0;
                    info.wchar = 0;
                }
                path = "/proc/" + std::to_string(pid) + "/smaps";
                std::ifstream smaps_file(path);
                if (smaps_file) {
                    while (std::getline(smaps_file, line)) {
                        std::istringstream smaps_iss(line);
                        std::string key;
                        unsigned long long value;
                        smaps_iss >> key >> value;
                        if (key == "Shared_Clean:") info.shared_clean += value;
                        else if (key == "Private_Dirty:") info.private_dirty += value;
                    }
                    smaps_file.close();
                } else {
                    info.shared_clean = 0;
                    info.private_dirty = 0;
                }
                path = "/proc/" + std::to_string(pid) + "/fd";
                DIR *fd_dir = opendir(path.c_str());
                info.fd_count = 0;
                if (fd_dir) {
                    while ((entry = readdir(fd_dir))) {
                        if (entry->d_type != DT_DIR) info.fd_count++;
                    }
                    closedir(fd_dir);
                }
                path = "/proc/" + std::to_string(pid) + "/net/dev";
                std::ifstream net_file(path);
                if (net_file) {
                    std::string line;
                    std::getline(net_file, line); // Header 1
                    std::getline(net_file, line); // Header 2
                    while (std::getline(net_file, line)) {
                        std::istringstream net_iss(line);
                        std::string interface;
                        unsigned long long rx_bytes, dummy_rx, tx_bytes, dummy_tx;
                        net_iss >> interface >> rx_bytes;
                        for (int i = 0; i < 7; ++i) net_iss >> dummy_rx;
                        net_iss >> tx_bytes;
                        info.net_rx_bytes += rx_bytes;
                        info.net_tx_bytes += tx_bytes;
                    }
                    net_file.close();
                } else {
                    info.net_rx_bytes = 0;
                    info.net_tx_bytes = 0;
                }
                processes.push_back(info);
                process_map[pid] = info;
                process_tree[info.ppid].push_back(pid);
            }
        }
        closedir(proc_dir);
        std::ifstream mem_file("/proc/meminfo");
        std::string mem_line;
        while (std::getline(mem_file, mem_line)) {
            std::istringstream mem_iss(mem_line);
            std::string key;
            unsigned long long value;
            mem_iss >> key >> value;
            if (key == "MemTotal:") mem_total = value;
        }
        mem_file.close();
        for (auto &proc : processes) {
            proc.mem_usage = 100.0 * static_cast<double>(proc.rss * getpagesize()) / (mem_total * 1024);
        }
        num_cores = sysconf(_SC_NPROCESSORS_ONLN);
        unsigned long long total_jiffies = 0, work_jiffies = 0;
        std::ifstream stat_file("/proc/stat");
        if (stat_file) {
            std::string line;
            std::getline(stat_file, line);
            std::istringstream iss(line);
            std::string cpu;
            unsigned long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
            iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;
            work_jiffies = user + nice + system + irq + softirq + steal + guest + guest_nice;
            total_jiffies = work_jiffies + idle + iowait;
            stat_file.close();
        }
        for (auto &proc : processes) {
            auto it = std::find_if(prev_processes.begin(), prev_processes.end(), [&](const ProcessInfo &p) { return p.pid == proc.pid; });
            if (it != prev_processes.end()) {
                unsigned long delta_utime = proc.utime - it->utime;
                unsigned long delta_stime = proc.stime - it->stime;
                unsigned long delta_process = delta_utime + delta_stime;
                unsigned long long delta_total = total_jiffies - prev_total_jiffies;
                if (delta_total > 0) {
                    proc.cpu_usage = 100.0 * static_cast<double>(delta_process) / delta_total * num_cores;
                } else {
                    proc.cpu_usage = 0.0;
                }
                unsigned long long delta_read = proc.read_bytes - it->read_bytes;
                unsigned long long delta_write = proc.write_bytes - it->write_bytes;
                proc.io_read_rate = static_cast<double>(delta_read) / 1024.0 / poll_interval;
                proc.io_write_rate = static_cast<double>(delta_write) / 1024.0 / poll_interval;
                unsigned long long delta_rx = proc.net_rx_bytes - it->net_rx_bytes;
                unsigned long long delta_tx = proc.net_tx_bytes - it->net_tx_bytes;
                proc.net_rx_rate = static_cast<double>(delta_rx) / 1024.0 / poll_interval;
                proc.net_tx_rate = static_cast<double>(delta_tx) / 1024.0 / poll_interval;
            } else {
                proc.cpu_usage = 0.0;
                proc.io_read_rate = 0.0;
                proc.io_write_rate = 0.0;
                proc.net_rx_rate = 0.0;
                proc.net_tx_rate = 0.0;
            }
        }
        prev_processes = processes;
        prev_total_jiffies = total_jiffies;
        prev_work_jiffies = work_jiffies;
    }

    void logProcesses() {
        if (!log_file.is_open()) {
            log_file.open("process_log.csv", std::ios::app);
            if (log_file) {
                log_file << "Timestamp,PID,PPID,State,Cmd,Mem%,CPU%,IO R (KB/s),IO W (KB/s),RChar (KB),WChar (KB),Shared (KB),Private (KB),FD,Threads,CtxtSw,Age (h),Priority,Nice,CPUs,Net R (KB/s),Net W (KB/s)\n";
            }
        }
        if (log_file) {
            time_t now = time(nullptr);
            std::string timestamp = ctime(&now);
            timestamp.erase(std::remove(timestamp.begin(), timestamp.end(), '\n'), timestamp.end());
            for (const auto &proc : processes) {
                log_file << timestamp << "," << proc.pid << "," << proc.ppid << "," << proc.state << "," << proc.cmd << "," << proc.mem_usage << "," << proc.cpu_usage << "," << proc.io_read_rate << "," << proc.io_write_rate << "," << proc.rchar / 1024 << "," << proc.wchar / 1024 << "," << proc.shared_clean << "," << proc.private_dirty << "," << proc.fd_count << "," << proc.num_threads << "," << proc.voluntary_ctxt_switches << "," << proc.process_age << "," << proc.priority << "," << proc.nice << "," << proc.cpus_allowed_list << "," << proc.net_rx_rate << "," << proc.net_tx_rate << "\n";
            }
            log_file.flush();
        }
    }

    void sortProcesses(std::string criterion) {
        if (criterion == "cpu") {
            std::sort(processes.begin(), processes.end(), [](const ProcessInfo &a, const ProcessInfo &b) {
                return a.cpu_usage > b.cpu_usage;
            });
        } else if (criterion == "mem") {
            std::sort(processes.begin(), processes.end(), [](const ProcessInfo &a, const ProcessInfo &b) {
                return a.mem_usage > b.mem_usage;
            });
        } else if (criterion == "io") {
            std::sort(processes.begin(), processes.end(), [](const ProcessInfo &a, const ProcessInfo &b) {
                return (a.io_read_rate + a.io_write_rate) > (b.io_read_rate + b.io_write_rate);
            });
        } else if (criterion == "net") {
            std::sort(processes.begin(), processes.end(), [](const ProcessInfo &a, const ProcessInfo &b) {
                return (a.net_rx_rate + a.net_tx_rate) > (b.net_rx_rate + b.net_tx_rate);
            });
        }
    }

    std::vector<ProcessInfo> filterProcesses(std::string filter_str) {
        std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(), ::tolower);
        std::vector<ProcessInfo> filtered;
        for (const auto &proc : processes) {
            std::string lower_cmd = proc.cmd;
            std::transform(lower_cmd.begin(), lower_cmd.end(), lower_cmd.begin(), ::tolower);
            if (lower_cmd.find(filter_str) != std::string::npos) {
                filtered.push_back(proc);
            }
        }
        return filtered;
    }

    void displayHeader() {
        wattron(win, COLOR_PAIR(1));
        mvwprintw(win, 0, 0, "System Uptime: %.2f h | MemTotal: %llu MB | Cores: %d | %s", system_uptime / 3600.0, mem_total / 1024, num_cores, logging_enabled ? "Logging: ON" : "Logging: OFF");
        mvwprintw(win, 1, 0, "F4: Filter | F5: Tree/List | F6: Sort | F9: Kill | L: Log | Q: Quit");
        wattroff(win, COLOR_PAIR(1));
    }

    void displayTree(pid_t pid, int depth, int &line, int max_lines, int max_cols) {
        if (line >= max_lines + scroll_offset || line < scroll_offset) return;
        auto it = process_map.find(pid);
        if (it == process_map.end()) return;
        if (line >= scroll_offset && line < max_lines + scroll_offset) {
            std::string indent(depth * 2, ' ');
            std::string cmd = it->second.cmd;
            if (cmd.length() > 20) cmd = cmd.substr(0, 17) + "...";
            int color = (it->second.state == 'Z') ? 3 : (it->second.ppid == 1 && it->second.pid != 1) ? 4 : (it->second.cpu_usage > 50.0) ? 2 : 1;
            if (line == selected_row) wattron(win, A_REVERSE);
            wattron(win, COLOR_PAIR(color));
            mvwprintw(win, line - scroll_offset + 3, 0, "%s%-5d %-5d %c %6.2f %6.2f %6.2f %6.2f %6llu %6llu %6llu %6llu %4llu %4ld %6llu %5.2f %3ld %3ld %-6s %6.2f %6.2f %-20s",
                      indent.c_str(), it->second.pid, it->second.ppid, it->second.state, it->second.mem_usage, it->second.cpu_usage,
                      it->second.io_read_rate, it->second.io_write_rate, it->second.rchar / 1024, it->second.wchar / 1024,
                      it->second.shared_clean, it->second.private_dirty, it->second.fd_count, it->second.num_threads,
                      it->second.voluntary_ctxt_switches, it->second.process_age, it->second.priority, it->second.nice,
                      it->second.cpus_allowed_list.c_str(), it->second.net_rx_rate, it->second.net_tx_rate, cmd.c_str());
            wattroff(win, COLOR_PAIR(color));
            if (line == selected_row) wattroff(win, A_REVERSE);
        }
        line++;
        auto children_it = process_tree.find(pid);
        if (children_it != process_tree.end()) {
            for (const auto &child : children_it->second) {
                displayTree(child, depth + 1, line, max_lines, max_cols);
            }
        }
    }

    void displayProcesses(int max_lines, int max_cols) {
        int line = 0;
        for (const auto &proc : processes) {
            if (line >= max_lines + scroll_offset || line < scroll_offset) {
                line++;
                continue;
            }
            std::string cmd = proc.cmd;
            if (cmd.length() > 20) cmd = cmd.substr(0, 17) + "...";
            int color = (proc.state == 'Z') ? 3 : (proc.ppid == 1 && proc.pid != 1) ? 4 : (proc.cpu_usage > 50.0) ? 2 : 1;
            if (line == selected_row) wattron(win, A_REVERSE);
            wattron(win, COLOR_PAIR(color));
            mvwprintw(win, line - scroll_offset + 3, 0, "%-5d %-5d %c %6.2f %6.2f %6.2f %6.2f %6llu %6llu %6llu %6llu %4llu %4ld %6llu %5.2f %3ld %3ld %-6s %6.2f %6.2f %-20s",
                      proc.pid, proc.ppid, proc.state, proc.mem_usage, proc.cpu_usage,
                      proc.io_read_rate, proc.io_write_rate, proc.rchar / 1024, proc.wchar / 1024,
                      proc.shared_clean, proc.private_dirty, proc.fd_count, proc.num_threads,
                      proc.voluntary_ctxt_switches, proc.process_age, proc.priority, proc.nice,
                      proc.cpus_allowed_list.c_str(), proc.net_rx_rate, proc.net_tx_rate, cmd.c_str());
            wattroff(win, COLOR_PAIR(color));
            if (line == selected_row) wattroff(win, A_REVERSE);
            line++;
        }
    }

    void handleInput() {
        int ch = getch();
        int max_lines = getmaxy(win) - 3;
        int total_lines = tree_view ? processes.size() : processes.size();
        switch (ch) {
            case KEY_UP:
                if (selected_row > 0) selected_row--;
                if (selected_row < scroll_offset) scroll_offset--;
                break;
            case KEY_DOWN:
                if (selected_row < total_lines - 1) selected_row++;
                if (selected_row >= scroll_offset + max_lines) scroll_offset++;
                break;
            case KEY_PPAGE:
                selected_row -= max_lines;
                scroll_offset -= max_lines;
                if (selected_row < 0) selected_row = 0;
                if (scroll_offset < 0) scroll_offset = 0;
                break;
            case KEY_NPAGE:
                if (selected_row + max_lines < total_lines) selected_row += max_lines;
                else selected_row = total_lines - 1;
                if (selected_row >= scroll_offset + max_lines) scroll_offset = selected_row - max_lines + 1;
                break;
            case KEY_HOME:
                selected_row = 0;
                scroll_offset = 0;
                break;
            case KEY_END:
                selected_row = total_lines - 1;
                if (selected_row >= max_lines) scroll_offset = selected_row - max_lines + 1;
                else scroll_offset = 0;
                break;
            case KEY_F(4): {
                char filter_input[256];
                echo();
                mvwprintw(win, 0, 0, "Filter cmd: ");
                wgetnstr(win, filter_input, 255);
                noecho();
                filter_str = std::string(filter_input);
                if (!filter_str.empty()) {
                    processes = filterProcesses(filter_str);
                } else {
                    scanProcesses();
                }
                selected_row = 0;
                scroll_offset = 0;
                break;
            }
            case KEY_F(5):
                tree_view = !tree_view;
                selected_row = 0;
                scroll_offset = 0;
                break;
            case KEY_F(6):
                sort_criterion = (sort_criterion == "cpu") ? "mem" : (sort_criterion == "mem") ? "io" : (sort_criterion == "io") ? "net" : "cpu";
                sortProcesses(sort_criterion);
                selected_row = 0;
                scroll_offset = 0;
                break;
            case KEY_F(9): {
                if (selected_row >= 0 && selected_row < processes.size()) {
                    auto proc = processes[selected_row];
                    if (proc.state == 'Z') {
                        mvwprintw(win, 0, 0, "Kill parent (PPID=%d)? (y/n): ", proc.ppid);
                        if (getch() == 'y') {
                            kill(proc.ppid, SIGTERM);
                            mvwprintw(win, 0, 0, "SIGTERM sent to parent.  ");
                        }
                    } else if (proc.ppid == 1 && proc.pid != 1) {
                        mvwprintw(win, 0, 0, "Kill orphan (PID=%d)? (y/n): ", proc.pid);
                        if (getch() == 'y') {
                            kill(proc.pid, SIGTERM);
                            mvwprintw(win, 0, 0, "SIGTERM sent to orphan.  ");
                        }
                    }
                }
                break;
            }
            case 'l':
            case 'L':
                logging_enabled = !logging_enabled;
                if (!logging_enabled && log_file.is_open()) {
                    log_file.close();
                }
                break;
            case 'q':
            case 'Q':
                endwin();
                if (log_file.is_open()) log_file.close();
                exit(0);
        }
    }

public:
    ProcessAnalyzer() {
        clk_tck = sysconf(_SC_CLK_TCK);
        std::ifstream uptime_file("/proc/uptime");
        if (uptime_file) {
            uptime_file >> system_uptime;
            uptime_file.close();
        }
        initscr();
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK); // Normal
        init_pair(2, COLOR_RED, COLOR_BLACK);   // High CPU
        init_pair(3, COLOR_YELLOW, COLOR_BLACK); // Zombie
        init_pair(4, COLOR_BLUE, COLOR_BLACK);  // Orphan
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        win = newwin(0, 0, 0, 0);
    }

    ~ProcessAnalyzer() {
        if (log_file.is_open()) log_file.close();
        endwin();
    }

    void run() {
        while (true) {
            scanProcesses();
            if (!filter_str.empty()) {
                processes = filterProcesses(filter_str);
            }
            if (!sort_criterion.empty()) {
                sortProcesses(sort_criterion);
            }
            if (logging_enabled) logProcesses();
            werase(win);
            displayHeader();
            mvwprintw(win, 2, 0, "%-5s %-5s %s %6s %6s %6s %6s %6s %6s %6s %6s %4s %4s %6s %5s %3s %3s %-6s %6s %6s %-20s",
                      "PID", "PPID", "S", "Mem%", "CPU%", "IO R", "IO W", "RChar", "WChar", "Shared", "Priv", "FD", "Thrd", "CtxtSw", "Age", "Pri", "Nice", "CPUs", "Net R", "Net W", "Cmd");
            int max_lines = getmaxy(win) - 3;
            int max_cols = getmaxx(win);
            if (tree_view) {
                int line = 0;
                displayTree(1, 0, line, max_lines, max_cols);
            } else {
                displayProcesses(max_lines, max_cols);
            }
            wrefresh(win);
            timeout(0);
            handleInput();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
};

int main() {
    ProcessAnalyzer analyzer;
    analyzer.run();
    return 0;
}
