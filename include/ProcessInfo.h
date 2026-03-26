#ifndef PROCESS_INFO_H
#define PROCESS_INFO_H

#include <string>
#include <vector>
#include <cstdint>
#include <sys/types.h>

struct ProcessInfo
{
    pid_t pid, ppid;
    char state;
    std::string cmd, cpus_allowed_list;
    long rss, num_threads, priority, nice;
    double mem_usage, cpu_usage, io_read_rate, io_write_rate, process_age, net_rx_rate, net_tx_rate;
    unsigned long utime, stime;
    uint64_t read_bytes, write_bytes, rchar, wchar, voluntary_ctxt_switches, shared_clean, private_dirty, fd_count, net_rx_bytes, net_tx_bytes;
};

struct SystemStats
{
    double cpu_usage, mem_usage, uptime;
    uint64_t mem_total, mem_free;
    int num_cores;
};

struct Filter
{
    std::string key, op, value;
    double numeric_val;
    long long_val;
};

#endif
