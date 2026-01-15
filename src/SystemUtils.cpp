#include "SystemUtils.h"
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <unistd.h>
#include <algorithm>
#include <cmath>

namespace SystemUtils {

void scanProcesses(std::vector<ProcessInfo>& processes, 
                    std::map<pid_t, std::vector<pid_t>>& process_tree, 
                    std::map<pid_t, ProcessInfo>& process_map,
                    uint64_t& mem_total, uint64_t& mem_free,
                    double& system_mem_usage, double& system_cpu_usage,
                    uint64_t& prev_total_jiffies, uint64_t& prev_work_jiffies,
                    const std::vector<ProcessInfo>& prev_processes,
                    int& num_cores, long clk_tck, double system_uptime,
                    double poll_interval, std::string& status_msg)
{
    processes.clear();
    process_tree.clear();
    process_map.clear();
    DIR *dir = opendir("/proc");
    if (!dir)
    {
        status_msg = "Error: Cannot open /proc";
        return;
    }
    struct dirent *entry;
    std::string line;
    while ((entry = readdir(dir)))
    {
        if (entry->d_type != DT_DIR || !isdigit(entry->d_name[0]))
        {
            continue;
        }
        pid_t pid = std::stoi(entry->d_name);
        ProcessInfo info;
        std::string path = "/proc/" + std::to_string(pid) + "/stat";
        std::ifstream stat_file(path);
        if (!stat_file)
        {
            continue;
        }
        std::getline(stat_file, line);
        size_t last_paren = line.find_last_of(')');
        if (last_paren == std::string::npos)
            continue;

        std::string pid_part = line.substr(0, line.find('('));
        info.pid = std::stoi(pid_part);

        size_t first_paren = line.find('(');
        info.cmd = line.substr(first_paren + 1, last_paren - first_paren - 1);

        std::string rest = line.substr(last_paren + 2);
        std::istringstream iss(rest);
        uint64_t starttime;
        long dummy_l;

        iss >> info.state >> info.ppid; // 3, 4
        for (int i = 0; i < 7; ++i)
            iss >> dummy_l;            // 5-11
        iss >> dummy_l >> dummy_l;     // 12, 13
        iss >> info.utime >> info.stime; // 14, 15
        iss >> dummy_l >> dummy_l;     // 16, 17
        iss >> info.priority >> info.nice; // 18, 19
        iss >> info.num_threads;       // 20
        iss >> dummy_l;                // 21
        iss >> starttime;              // 22

        info.process_age = (system_uptime - static_cast<double>(starttime) / clk_tck) / 3600.0;
        stat_file.close();

        info.read_bytes = 0;
        info.write_bytes = 0;
        info.rchar = 0;
        info.wchar = 0;
        info.net_rx_bytes = 0;
        info.net_tx_bytes = 0;
        info.shared_clean = 0;
        info.private_dirty = 0;
        info.fd_count = 0;

        path = "/proc/" + std::to_string(pid) + "/status";
        std::ifstream status_file(path);
        if (status_file)
        {
            while (std::getline(status_file, line))
            {
                std::istringstream iss_status(line);
                std::string key;
                iss_status >> key;
                if (key == "VmRSS:")
                {
                    iss_status >> info.rss;
                }
                else if (key == "voluntary_ctxt_switches:")
                {
                    iss_status >> info.voluntary_ctxt_switches;
                }
                else if (key == "Cpus_allowed_list:")
                {
                    iss_status >> info.cpus_allowed_list;
                }
            }
            status_file.close();
        }
        path = "/proc/" + std::to_string(pid) + "/io";
        std::ifstream io_file(path);
        if (io_file)
        {
            while (std::getline(io_file, line))
            {
                std::istringstream iss_io(line);
                std::string key;
                iss_io >> key;
                if (key == "read_bytes:")
                {
                    iss_io >> info.read_bytes;
                }
                else if (key == "write_bytes:")
                {
                    iss_io >> info.write_bytes;
                }
                else if (key == "rchar:")
                {
                    iss_io >> info.rchar;
                }
                else if (key == "wchar:")
                {
                    iss_io >> info.wchar;
                }
            }
            io_file.close();
        }
        path = "/proc/" + std::to_string(pid) + "/smaps";
        std::ifstream smaps_file(path);
        if (smaps_file)
        {
            while (std::getline(smaps_file, line))
            {
                std::istringstream iss_smaps(line);
                std::string key;
                uint64_t value;
                iss_smaps >> key >> value;
                if (key == "Shared_Clean:")
                {
                    info.shared_clean += value;
                }
                else if (key == "Private_Dirty:")
                {
                    info.private_dirty += value;
                }
            }
            smaps_file.close();
        }
        path = "/proc/" + std::to_string(pid) + "/fd";
        DIR *fd_dir = opendir(path.c_str());
        if (fd_dir)
        {
            struct dirent *fd_entry;
            while ((fd_entry = readdir(fd_dir)))
            {
                if (fd_entry->d_type != DT_DIR)
                {
                    info.fd_count++;
                }
            }
            closedir(fd_dir);
        }
        path = "/proc/" + std::to_string(pid) + "/net/dev";
        std::ifstream net_file(path);
        if (net_file)
        {
            std::getline(net_file, line);
            std::getline(net_file, line);
            while (std::getline(net_file, line))
            {
                std::istringstream iss_net(line);
                std::string interface;
                uint64_t rx_bytes, dummy, tx_bytes;
                iss_net >> interface >> rx_bytes;
                for (int i = 0; i < 7; i++)
                {
                    iss_net >> dummy;
                }
                iss_net >> tx_bytes;
                info.net_rx_bytes += rx_bytes;
                info.net_tx_bytes += tx_bytes;
            }
            net_file.close();
        }
        processes.push_back(info);
        process_map[pid] = info;
        process_tree[info.ppid].push_back(pid);
    }
    closedir(dir);
    
    std::ifstream mem_file("/proc/meminfo");
    while (std::getline(mem_file, line))
    {
        std::istringstream iss_mem(line);
        std::string key;
        uint64_t value;
        iss_mem >> key >> value;
        if (key == "MemTotal:")
        {
            mem_total = value;
        }
        else if (key == "MemFree:")
        {
            mem_free = value;
        }
    }
    mem_file.close();
    system_mem_usage = 100.0 * (1.0 - static_cast<double>(mem_free) / mem_total);
    for (auto &proc : processes)
    {
        proc.mem_usage = 100.0 * static_cast<double>(proc.rss * getpagesize()) / (mem_total * 1024);
    }
    num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    uint64_t total_jiffies = 0, work_jiffies = 0;
    std::ifstream stat_file("/proc/stat");
    if (stat_file)
    {
        std::getline(stat_file, line);
        std::istringstream iss_stat(line);
        std::string cpu;
        uint64_t user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
        iss_stat >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;
        work_jiffies = user + nice + system + irq + softirq + steal + guest + guest_nice;
        total_jiffies = work_jiffies + idle + iowait;
        stat_file.close();
    }
    if (prev_total_jiffies > 0)
    {
        system_cpu_usage = 100.0 * static_cast<double>(work_jiffies - prev_work_jiffies) / (total_jiffies - prev_total_jiffies);
    }
    else
    {
        system_cpu_usage = 0.0;
    }
    for (auto &proc : processes)
    {
        auto it = std::find_if(prev_processes.begin(), prev_processes.end(), [&](const ProcessInfo &p)
                               { return p.pid == proc.pid; });
        if (it != prev_processes.end())
        {
            unsigned long delta_utime = proc.utime - it->utime;
            unsigned long delta_stime = proc.stime - it->stime;
            unsigned long delta_process = delta_utime + delta_stime;
            uint64_t delta_total = total_jiffies - prev_total_jiffies;
            if (delta_total > 0)
            {
                proc.cpu_usage = 100.0 * static_cast<double>(delta_process) / delta_total * num_cores;
            }
            else
            {
                proc.cpu_usage = 0.0;
            }
            uint64_t delta_read = proc.read_bytes - it->read_bytes;
            uint64_t delta_write = proc.write_bytes - it->write_bytes;
            proc.io_read_rate = static_cast<double>(delta_read) / 1024.0 / poll_interval;
            proc.io_write_rate = static_cast<double>(delta_write) / 1024.0 / poll_interval;
            uint64_t delta_rx = proc.net_rx_bytes - it->net_rx_bytes;
            uint64_t delta_tx = proc.net_tx_bytes - it->net_tx_bytes;
            proc.net_rx_rate = static_cast<double>(delta_rx) / 1024.0 / poll_interval;
            proc.net_tx_rate = static_cast<double>(delta_tx) / 1024.0 / poll_interval;
        }
        else
        {
            proc.cpu_usage = 0.0;
            proc.io_read_rate = 0.0;
            proc.io_write_rate = 0.0;
            proc.net_rx_rate = 0.0;
            proc.net_tx_rate = 0.0;
        }
    }
    prev_total_jiffies = total_jiffies;
    prev_work_jiffies = work_jiffies;
}

double getUptime() {
    std::ifstream uptime_file("/proc/uptime");
    double uptime = 0.0;
    if (uptime_file)
    {
        uptime_file >> uptime;
        uptime_file.close();
    }
    return uptime;
}

} // namespace SystemUtils
