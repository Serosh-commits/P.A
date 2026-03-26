#include "ProcessSorter.h"
#include <algorithm>

namespace ProcessSorter {

void sortProcesses(std::vector<ProcessInfo>& processes, const std::string& criterion)
{
    if (criterion == "cpu")
    {
        std::sort(processes.begin(), processes.end(), [](const ProcessInfo &a, const ProcessInfo &b) {
            if (std::abs(a.cpu_usage - b.cpu_usage) < 0.001) return a.pid < b.pid;
            return a.cpu_usage > b.cpu_usage;
        });
    }
    else if (criterion == "mem")
    {
        std::sort(processes.begin(), processes.end(), [](const ProcessInfo &a, const ProcessInfo &b) {
            if (std::abs(a.mem_usage - b.mem_usage) < 0.001) return a.pid < b.pid;
            return a.mem_usage > b.mem_usage;
        });
    }
    else if (criterion == "io")
    {
        std::sort(processes.begin(), processes.end(), [](const ProcessInfo &a, const ProcessInfo &b) {
            double a_io = a.io_read_rate + a.io_write_rate;
            double b_io = b.io_read_rate + b.io_write_rate;
            if (std::abs(a_io - b_io) < 0.001) return a.pid < b.pid;
            return a_io > b_io;
        });
    }
    else if (criterion == "net")
    {
        std::sort(processes.begin(), processes.end(), [](const ProcessInfo &a, const ProcessInfo &b) {
            double a_net = a.net_rx_rate + a.net_tx_rate;
            double b_net = b.net_rx_rate + b.net_tx_rate;
            if (std::abs(a_net - b_net) < 0.001) return a.pid < b.pid;
            return a_net > b_net;
        });
    }
}

}
