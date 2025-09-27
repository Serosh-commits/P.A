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

    void detectAndHandle() {
        std::vector<pid_t> zombies;
        std::vector<pid_t> orphans;
        for (const auto &proc : processes) {
            if (proc.state == 'Z') {
                zombies.push_back(proc.pid);
            }
            if (proc.ppid == 1 && proc.pid != 1) {
                orphans.push_back(proc.pid);
            }
        }
        for (const auto &z_pid : zombies) {
            auto it = process_map.find(z_pid);
            if (it != process_map.end()) {
                std::cout << "Zombie process: PID=" << z_pid << ", Cmd=" << it->second.cmd << std::endl;
                std::cout << "Kill parent (PPID=" << it->second.ppid << ")? (y/n): ";
                char confirm;
                std::cin >> confirm;
                if (confirm == 'y' || confirm == 'Y') {
                    kill(it->second.ppid, SIGTERM);
                    std::cout << "SIGTERM sent to parent.\n";
                }
            }
        }
        for (const auto &o_pid : orphans) {
            auto it = process_map.find(o_pid);
            if (it != process_map.end()) {
                std::cout << "Orphan process: PID=" << o_pid << ", Cmd=" << it->second.cmd << std::endl;
                std::cout << "Kill orphan? (y/n): ";
                char confirm;
                std::cin >> confirm;
                if (confirm == 'y' || confirm == 'Y') {
                    kill(o_pid, SIGTERM);
                    std::cout << "SIGTERM sent to orphan.\n";
                }
            }
        }
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

    void displayTree(pid_t pid, int depth = 0) {
        auto it = process_map.find(pid);
        if (it == process_map.end()) return;
        for (int i = 0; i < depth; ++i) std::cout << "  ";
        std::cout << "PID:" << pid
                  << " PPID:" << it->second.ppid
                  << " State:" << it->second.state
                  << " Mem%:" << std::fixed << std::setprecision(2) << it->second.mem_usage
                  << " CPU%:" << it->second.cpu_usage
                  << " IO R:" << it->second.io_read_rate << "KB/s"
                  << " W:" << it->second.io_write_rate << "KB/s"
                  << " RChar:" << it->second.rchar / 1024 << "KB"
                  << " WChar:" << it->second.wchar / 1024 << "KB"
                  << " Shared:" << it->second.shared_clean << "KB"
                  << " Private:" << it->second.private_dirty << "KB"
                  << " FD:" << it->second.fd_count
                  << " Threads:" << it->second.num_threads
                  << " CtxtSw:" << it->second.voluntary_ctxt_switches
                  << " Age:" << it->second.process_age << "h"
                  << " Pri:" << it->second.priority
                  << " Nice:" << it->second.nice
                  << " CPUs:" << it->second.cpus_allowed_list
                  << " Net R:" << it->second.net_rx_rate << "KB/s"
                  << " W:" << it->second.net_tx_rate << "KB/s"
                  << " Cmd:" << it->second.cmd << std::endl;
        auto children_it = process_tree.find(pid);
        if (children_it != process_tree.end()) {
            for (const auto &child : children_it->second) {
                displayTree(child, depth + 1);
            }
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
    }

    void run() {
        while (true) {
            scanProcesses();
            system("clear");
            std::cout << "Enter sort (c=cpu, m=mem, i=io, n=net) or filter cmd substring (f=string), log (l=y/n): ";
            std::string input;
            std::getline(std::cin, input);
            std::string sort_choice, filter_str, log_choice;
            if (input == "c" || input == "m" || input == "i" || input == "n") {
                sort_choice = input;
            } else if (input.substr(0, 2) == "f=") {
                filter_str = input.substr(2);
            } else if (input == "l=y") {
                log_choice = "y";
            }
            if (!filter_str.empty()) {
                auto filtered = filterProcesses(filter_str);
                processes = filtered;
            }
            if (!sort_choice.empty()) {
                sortProcesses(sort_choice);
            }
            std::cout << "Process Tree:\n";
            if (!sort_choice.empty()) {
                for (const auto &proc : processes) {
                    displayTree(proc.pid, 0);
                }
            } else {
                displayTree(1);
            }
            if (log_choice == "y") logProcesses();
            detectAndHandle();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
};

int main() {
    ProcessAnalyzer analyzer;
    analyzer.run();
    return 0;
}
