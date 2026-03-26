#include "SystemUtils.h"
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>

#ifndef PF_KTHREAD
#define PF_KTHREAD 0x00200000
#endif

namespace SystemUtils {

void scanProcesses(std::vector<ProcessInfo>& processes, std::map<pid_t, std::vector<pid_t>>& process_tree,
                        std::map<pid_t, ProcessInfo>& process_map, uint64_t& mem_total, uint64_t& mem_free,
                        double& system_mem_usage, double& system_cpu_usage, uint64_t& prev_total_jiffies,
                        uint64_t& prev_work_jiffies, std::map<pid_t, ProcessInfo>& prev_processes,
                        int num_cores, long clk_tck, double system_uptime, double poll_interval,
                        std::string& /*status_msg*/, CPULoadBreakdown& b, MemBreakdown& m)
{
    processes.clear();
    process_tree.clear();
    process_map.clear();

    m = {};
    {
        FILE* mf = fopen("/proc/meminfo", "r");
        if (mf) {
            char ml[128];
            while (fgets(ml, sizeof(ml), mf)) {
                unsigned long val = 0;
                if      (sscanf(ml, "MemTotal: %lu", &val) == 1) m.total = val;
                else if (sscanf(ml, "MemFree: %lu",  &val) == 1) m.free  = val;
                else if (sscanf(ml, "Buffers: %lu",  &val) == 1) m.buffers = val;
                else if (sscanf(ml, "Cached: %lu",   &val) == 1) m.cached  = val;
                else if (sscanf(ml, "SReclaimable: %lu", &val) == 1) m.s_reclaimable = val;
            }
            fclose(mf);
        }
    }
    mem_total = m.total;
    mem_free  = m.free;
    if (m.total > 0) {
        uint64_t cached_all = m.cached + m.s_reclaimable;
        m.shorthand_used = m.total - m.free - m.buffers - cached_all;
        system_mem_usage = 100.0 * (double)(m.total - m.free) / (double)m.total;
    }

    uint64_t total_jiffies = 0, work_jiffies = 0;
    {
        FILE* psf = fopen("/proc/stat", "r");
        if (psf) {
            char line[256] = {};
            fgets(line, sizeof(line), psf);
            fclose(psf);
            uint64_t u=0,n=0,s=0,i=0,iw=0,ir=0,si=0,st=0,g=0,gn=0;
            if (sscanf(line, "cpu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                   &u,&n,&s,&i,&iw,&ir,&si,&st,&g,&gn) >= 4) 
            {
                work_jiffies  = u + n + s + ir + si + st + g + gn;
                total_jiffies = work_jiffies + i + iw;

                if (prev_total_jiffies > 0 && total_jiffies > prev_total_jiffies) {
                    uint64_t dt = total_jiffies - prev_total_jiffies;
                    b.user    = 100.0 * (u - b.prev_u) / dt;
                    b.nice    = 100.0 * (n - b.prev_n) / dt;
                    b.sys     = 100.0 * (s - b.prev_s) / dt;
                    b.idle    = 100.0 * (i - b.prev_i) / dt;
                    b.iowait  = 100.0 * (iw - b.prev_iw) / dt;
                    b.irq     = 100.0 * (ir - b.prev_ir) / dt;
                    b.softirq = 100.0 * (si - b.prev_si) / dt;
                    b.steal   = 100.0 * (st - b.prev_st) / dt;
                }
                b.prev_u=u; b.prev_n=n; b.prev_s=s; b.prev_i=i;
                b.prev_iw=iw; b.prev_ir=ir; b.prev_si=si; b.prev_st=st;
            }
        }
    }
    uint64_t delta_t = 1;
    if (prev_total_jiffies > 0 && total_jiffies > prev_total_jiffies) {
        delta_t = total_jiffies - prev_total_jiffies;
        double dcpu = 100.0 * (double)(work_jiffies - prev_work_jiffies) / (double)delta_t;
        system_cpu_usage = (dcpu < 0) ? 0.0 : (dcpu > 100.0 ? 100.0 : dcpu);
    }

    DIR* dir = opendir("/proc");
    if (!dir) return;

    char buf[4096];
    struct dirent* entry;

    while ((entry = readdir(dir)))
    {
        if (entry->d_type != DT_DIR) continue;
        char* endp;
        pid_t pid = (pid_t)strtol(entry->d_name, &endp, 10);
        if (*endp != '\0' || pid <= 0) continue;

        ProcessInfo info = {};
        info.pid = pid;

        snprintf(buf, sizeof(buf), "/proc/%d/stat", pid);
        FILE* sf = fopen(buf, "r");
        if (!sf) continue;
        char statline[2048] = {};
        bool ok = (fgets(statline, sizeof(statline), sf) != NULL);
        fclose(sf);
        if (!ok) continue;

        char* fp = strchr(statline, '(');
        char* lp = strrchr(statline, ')');
        if (!fp || !lp || fp >= lp) continue;
        info.cmd = std::string(fp + 1, lp - fp - 1);

        char   state = 'S';
        long   ppid = 0, priority = 20, nice = 0, num_threads = 1, rss = 0;
        unsigned long utime = 0, stime = 0, starttime = 0, flags = 0;

        {
            char* p = lp + 1;
            while (*p == ' ') p++;
            state = *p; p++;
            if (*p == ' ') p++;

            auto skip = [](char* p) { while (*p && *p != ' ') p++; while (*p == ' ') p++; return p; };
            ppid = strtol(p, &p, 10); while (*p == ' ') p++;
            skip(p); p = skip(p);
            skip(p); p = skip(p);
            skip(p); p = skip(p);
            skip(p); p = skip(p);
            flags = strtoul(p, &p, 10); while (*p == ' ') p++;
            skip(p); p = skip(p);
            skip(p); p = skip(p);
            skip(p); p = skip(p);
            skip(p); p = skip(p);
            utime = strtoul(p, &p, 10); while (*p == ' ') p++;
            stime = strtoul(p, &p, 10); while (*p == ' ') p++;
            skip(p); p = skip(p);
            skip(p); p = skip(p);
            priority = strtol(p, &p, 10); while (*p == ' ') p++;
            nice     = strtol(p, &p, 10); while (*p == ' ') p++;
            num_threads = strtol(p, &p, 10); while (*p == ' ') p++;
            skip(p); p = skip(p);
            starttime = strtoul(p, &p, 10); while (*p == ' ') p++;
            skip(p); p = skip(p);
            rss = strtol(p, &p, 10);
        }

        info.state       = state;
        info.ppid        = (pid_t)ppid;
        info.utime       = utime;
        info.stime       = stime;
        info.priority    = priority;
        info.nice        = nice;
        info.num_threads = num_threads;
        info.rss         = rss * (getpagesize() / 1024);
        bool is_kthread  = (flags & PF_KTHREAD) != 0;

        if (system_uptime > 0 && clk_tck > 0) {
            info.process_age = (system_uptime - (double)starttime / (double)clk_tck) / 3600.0;
            if (info.process_age < 0) info.process_age = 0;
        }

        snprintf(buf, sizeof(buf), "/proc/%d/status", pid);
        FILE* stf = fopen(buf, "r");
        if (stf) {
            char sl[256];
            while (fgets(sl, sizeof(sl), stf)) {
                unsigned long val = 0;
                if (sscanf(sl, "voluntary_ctxt_switches:\t%lu", &val) == 1)
                    info.voluntary_ctxt_switches = val;
            }
            fclose(stf);
        }

        if (!is_kthread) {
            snprintf(buf, sizeof(buf), "/proc/%d/io", pid);
            FILE* iof = fopen(buf, "r");
            if (iof) {
                char il[128];
                while (fgets(il, sizeof(il), iof)) {
                    unsigned long val = 0;
                    if      (sscanf(il, "rchar: %lu",       &val) == 1) info.rchar       = val;
                    else if (sscanf(il, "wchar: %lu",       &val) == 1) info.wchar       = val;
                    else if (sscanf(il, "read_bytes: %lu",  &val) == 1) info.read_bytes  = val;
                    else if (sscanf(il, "write_bytes: %lu", &val) == 1) info.write_bytes = val;
                }
                fclose(iof);
            }

            snprintf(buf, sizeof(buf), "/proc/%d/net/dev", pid);
            FILE* nf = fopen(buf, "r");
            if (nf) {
                char nl[512];
                fgets(nl, sizeof(nl), nf);
                fgets(nl, sizeof(nl), nf);
                while (fgets(nl, sizeof(nl), nf)) {
                    char* colon = strchr(nl, ':');
                    if (!colon) continue;
                    uint64_t rx=0, tx=0, d=0;
                    int nr = sscanf(colon+1, " %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                                    &rx,&d,&d,&d,&d,&d,&d,&d,&tx);
                    if (nr >= 9) { info.net_rx_bytes += rx; info.net_tx_bytes += tx; }
                }
                fclose(nf);
            }

            snprintf(buf, sizeof(buf), "/proc/%d/smaps_rollup", pid);
            FILE* rmf = fopen(buf, "r");
            if (rmf) {
                char rl[256];
                while (fgets(rl, sizeof(rl), rmf)) {
                    unsigned long val = 0;
                    if      (sscanf(rl, "Shared_Clean: %lu",  &val) == 1) info.shared_clean  = val;
                    else if (sscanf(rl, "Private_Dirty: %lu", &val) == 1) info.private_dirty = val;
                }
                fclose(rmf);
            }

            snprintf(buf, sizeof(buf), "/proc/%d/fd", pid);
            DIR* fdir = opendir(buf);
            if (fdir) {
                uint64_t cnt = 0;
                while (readdir(fdir)) cnt++;
                info.fd_count = (cnt >= 2) ? cnt - 2 : 0;
                closedir(fdir);
            }
        }

        processes.push_back(info);
        process_map[pid] = info;
        process_tree[info.ppid].push_back(pid);
    }
    closedir(dir);

    for (auto& proc : processes) {
        if (m.total > 0)
            proc.mem_usage = 100.0 * (double)proc.rss / (double)m.total;

        auto it = prev_processes.find(proc.pid);
        if (it == prev_processes.end()) continue;
        const ProcessInfo& prev = it->second;

        uint64_t delta_p = (proc.utime + proc.stime) - (prev.utime + prev.stime);
        proc.cpu_usage = 100.0 * (double)delta_p / (double)delta_t * num_cores;
        if (proc.cpu_usage < 0) proc.cpu_usage = 0;
        if (proc.cpu_usage > 100.0 * num_cores) proc.cpu_usage = 100.0 * num_cores;

        bool has_disk_io = (proc.read_bytes > 0 || prev.read_bytes > 0);
        if (has_disk_io) {
            proc.io_read_rate  = (double)(proc.read_bytes  - prev.read_bytes)  / 1024.0 / poll_interval;
            proc.io_write_rate = (double)(proc.write_bytes - prev.write_bytes) / 1024.0 / poll_interval;
        } else if (proc.rchar > 0 || prev.rchar > 0) {
            proc.io_read_rate  = (double)(proc.rchar - prev.rchar) / 1024.0 / poll_interval;
            proc.io_write_rate = (double)(proc.wchar - prev.wchar) / 1024.0 / poll_interval;
        }
        if (proc.io_read_rate  < 0) proc.io_read_rate  = 0;
        if (proc.io_write_rate < 0) proc.io_write_rate = 0;

        if (proc.net_rx_bytes >= prev.net_rx_bytes)
            proc.net_rx_rate = (double)(proc.net_rx_bytes - prev.net_rx_bytes) / 1024.0 / poll_interval;
        if (proc.net_tx_bytes >= prev.net_tx_bytes)
            proc.net_tx_rate = (double)(proc.net_tx_bytes - prev.net_tx_bytes) / 1024.0 / poll_interval;
    }

    prev_total_jiffies = total_jiffies;
    prev_work_jiffies  = work_jiffies;
}

double getUptime() {
    FILE* f = fopen("/proc/uptime", "r");
    double u = 0;
    if (f) { fscanf(f, "%lf", &u); fclose(f); }
    return u;
}

}
