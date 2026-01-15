#ifndef SYSTEM_UTILS_H
#define SYSTEM_UTILS_H

#include "ProcessInfo.h"
#include <vector>
#include <map>
#include <string>

namespace SystemUtils {
    void scanProcesses(std::vector<ProcessInfo>& processes, 
                        std::map<pid_t, std::vector<pid_t>>& process_tree, 
                        std::map<pid_t, ProcessInfo>& process_map,
                        uint64_t& mem_total, uint64_t& mem_free,
                        double& system_mem_usage, double& system_cpu_usage,
                        uint64_t& prev_total_jiffies, uint64_t& prev_work_jiffies,
                        const std::vector<ProcessInfo>& prev_processes,
                        int& num_cores, long clk_tck, double system_uptime,
                        double poll_interval, std::string& status_msg);
    
    double getUptime();
}

#endif // SYSTEM_UTILS_H
