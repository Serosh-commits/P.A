#include "ProcessSorter.h"
#include <algorithm>

namespace ProcessSorter {

void sortProcesses(std::vector<ProcessInfo>& processes, const std::string& criterion)
{
    if (criterion == "cpu")
    {
        std::sort(processes.begin(), processes.end(), [](const ProcessInfo &a, const ProcessInfo &b)
                  { return a.cpu_usage > b.cpu_usage; });
    }
    else if (criterion == "mem")
    {
        std::sort(processes.begin(), processes.end(), [](const ProcessInfo &a, const ProcessInfo &b)
                  { return a.mem_usage > b.mem_usage; });
    }
    else if (criterion == "io")
    {
        std::sort(processes.begin(), processes.end(), [](const ProcessInfo &a, const ProcessInfo &b)
                  { return (a.io_read_rate + a.io_write_rate) > (b.io_read_rate + b.io_write_rate); });
    }
    else if (criterion == "net")
    {
        std::sort(processes.begin(), processes.end(), [](const ProcessInfo &a, const ProcessInfo &b)
                  { return (a.net_rx_rate + a.net_tx_rate) > (b.net_rx_rate + b.net_tx_rate); });
    }
}

} // namespace ProcessSorter
