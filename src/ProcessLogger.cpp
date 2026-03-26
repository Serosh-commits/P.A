#include "ProcessLogger.h"
#include <ctime>
#include <algorithm>

namespace ProcessLogger {

void logProcesses(std::ofstream& log_file, const std::vector<ProcessInfo>& processes, std::string& status_msg)
{
    if (!log_file.is_open())
    {
        log_file.open("process_log.csv", std::ios::app);
        if (!log_file)
        {
            status_msg = "Error: Cannot open process_log.csv";
            return;
        }
        log_file << "Timestamp,PID,PPID,State,Cmd,Mem%,CPU%,IO R (KB/s),IO W (KB/s),RChar (KB),WChar (KB),Shared (KB),Private (KB),FD,Threads,CtxtSw,Age (h),Priority,Nice,CPUs,Net R (KB/s),Net W (KB/s)\n";
    }
    if (log_file)
    {
        time_t now = time(nullptr);
        std::string ts = ctime(&now);
        ts.erase(std::remove(ts.begin(), ts.end(), '\n'), ts.end());
        for (const auto &proc : processes)
        {
            log_file << ts << "," << proc.pid << "," << proc.ppid << "," << proc.state << "," << proc.cmd << "," << proc.mem_usage << "," << proc.cpu_usage << "," << proc.io_read_rate << "," << proc.io_write_rate << "," << proc.rchar / 1024 << "," << proc.wchar / 1024 << "," << proc.shared_clean << "," << proc.private_dirty << "," << proc.fd_count << "," << proc.num_threads << "," << proc.voluntary_ctxt_switches << "," << proc.process_age << "," << proc.priority << "," << proc.nice << "," << proc.cpus_allowed_list << "," << proc.net_rx_rate << "," << proc.net_tx_rate << "\n";
        }
        log_file.flush();
    }
}

}
