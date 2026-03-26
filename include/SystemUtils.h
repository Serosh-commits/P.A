#ifndef SYSTEM_UTILS_H
#define SYSTEM_UTILS_H

#include "ProcessInfo.h"
#include <vector>
#include <map>
#include <string>

namespace SystemUtils {
    struct CPULoadBreakdown {
        double user;
        double nice;
        double sys;
        double idle;
        double iowait;
        double irq;
        double softirq;
        double steal;
        double total;
        
        uint64_t prev_u, prev_n, prev_s, prev_i, prev_iw, prev_ir, prev_si, prev_st;
    };

    struct MemBreakdown {
        uint64_t total;
        uint64_t free;
        uint64_t buffers;
        uint64_t cached;
        uint64_t s_reclaimable;
        uint64_t shorthand_used;
    };

    void scanProcesses(std::vector<ProcessInfo>& processes, 
                        std::map<pid_t, std::vector<pid_t>>& process_tree, 
                        std::map<pid_t, ProcessInfo>& process_map,
                        uint64_t& mem_total, uint64_t& mem_free,
                        double& system_mem_usage, double& system_cpu_usage,
                        uint64_t& prev_total_jiffies, uint64_t& prev_work_jiffies,
                        std::map<pid_t, ProcessInfo>& prev_processes,
                        int num_cores, long clk_tck, double system_uptime,
                        double poll_interval, std::string& status_msg,
                        CPULoadBreakdown& cpu_breakdown,
                        MemBreakdown& mem_breakdown);
    
    double getUptime();
}

#endif
