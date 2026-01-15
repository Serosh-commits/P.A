#include "DisplayEngine.h"
#include <cmath>
#include <algorithm>

namespace DisplayEngine {

void displayHeader(WINDOW* win, uint64_t mem_total, uint64_t mem_free, 
                    double system_cpu_usage, double system_mem_usage, 
                    double system_uptime, int num_cores, 
                    const std::vector<Filter>& filters, bool logging_enabled, 
                    const std::string& sort_criterion, const std::string& status_msg)
{
    double load[3];
    getloadavg(load, 3);

    wattron(win, COLOR_PAIR(1));
    std::string filter_display = filters.empty() ? "None" : "";
    for (int i = 0; i < (int)filters.size(); i++)
    {
        if (i > 0)
        {
            filter_display += " ";
        }
        filter_display += filters[i].key + filters[i].op + filters[i].value;
    }

    double used_mem = (double)(mem_total - mem_free) / 1024.0 / 1024.0;
    double total_mem = (double)mem_total / 1024.0 / 1024.0;

    mvwprintw(win, 0, 0, "CPU: %.1f%% | Mem: %.1f%% (%.1f/%.1f GB) | Load: %.2f %.2f %.2f | Uptime: %.2f h | Cores: %d",
              system_cpu_usage, system_mem_usage, used_mem, total_mem, load[0], load[1], load[2], system_uptime / 3600.0, num_cores);

    mvwprintw(win, 1, 0, "Log: %s | Sort: %s | Filter: %s",
              logging_enabled ? "ON" : "OFF", sort_criterion.empty() ? "None" : sort_criterion.c_str(), filter_display.c_str());

    mvwprintw(win, 2, 0, "F4: Filter | F5: Tree/List | F6: Sort | F9: Kill | Z: Zombies | L: Log | Q: Quit");
    if (!status_msg.empty())
    {
        wattron(win, COLOR_PAIR(2));
        mvwprintw(win, 2, 0, "%s", status_msg.c_str());
        wattroff(win, COLOR_PAIR(2));
    }
    wattroff(win, COLOR_PAIR(1));
}

void displayTree(WINDOW* win, pid_t pid, int depth, int &line, int max_lines, 
                    int scroll_offset, int selected_row, 
                    const std::map<pid_t, ProcessInfo>& process_map, 
                    const std::map<pid_t, std::vector<pid_t>>& process_tree)
{    
    auto it = process_map.find(pid);
    if (it == process_map.end())
    {
        return;
    }
    
    if (line >= scroll_offset && line < max_lines + scroll_offset)
    {
        std::string indent(depth * 2, ' ');
        std::string cmd = it->second.cmd;
        if (cmd.length() > 20)
        {
            cmd = cmd.substr(0, 17) + "...";
        }
        if (line == selected_row)
        {
            wattron(win, A_REVERSE);
        }
        double io_read = (it->second.io_read_rate < 0 || std::isnan(it->second.io_read_rate)) ? 0.0 : it->second.io_read_rate;
        double io_write = (it->second.io_write_rate < 0 || std::isnan(it->second.io_write_rate)) ? 0.0 : it->second.io_write_rate;
        double net_rx = (it->second.net_rx_rate < 0 || std::isnan(it->second.net_rx_rate)) ? 0.0 : it->second.net_rx_rate;
        double net_tx = (it->second.net_tx_rate < 0 || std::isnan(it->second.net_tx_rate)) ? 0.0 : it->second.net_tx_rate;

        mvwprintw(win, line - scroll_offset + 4, 0, "%s%-5d %-5d %c %6.1f %6.1f %6.1f %6.1f %6llu %6llu %6llu %6llu %4llu %4ld %6llu %5.1f %3ld %3ld %-6s %6.1f %6.1f %-20s",
                  indent.c_str(), it->second.pid, it->second.ppid, it->second.state, it->second.mem_usage, it->second.cpu_usage,
                  io_read, io_write, (unsigned long long)(it->second.rchar / 1024), (unsigned long long)(it->second.wchar / 1024),
                  (unsigned long long)it->second.shared_clean, (unsigned long long)it->second.private_dirty, (unsigned long long)it->second.fd_count, it->second.num_threads,
                  (unsigned long long)it->second.voluntary_ctxt_switches, it->second.process_age, it->second.priority, it->second.nice,
                  it->second.cpus_allowed_list.c_str(), net_rx, net_tx, cmd.c_str());
        if (line == selected_row)
        {
            wattroff(win, A_REVERSE);
        }
    }
    line++;
    auto children = process_tree.find(pid);
    if (children != process_tree.end())
    {
        for (const auto &child : children->second)
        {
            displayTree(win, child, depth + 1, line, max_lines, scroll_offset, selected_row, process_map, process_tree);
        }
    }
}

void displayProcesses(WINDOW* win, int max_lines, int scroll_offset, 
                        int selected_row, const std::vector<ProcessInfo>& processes)
{
    int line = 0;
    for (const auto &proc : processes)
    {
        if (line >= max_lines + scroll_offset || line < scroll_offset)
        {
            line++;
            continue;
        }
        std::string cmd = proc.cmd;
        if (cmd.length() > 20)
        {
            cmd = cmd.substr(0, 17) + "...";
        }
        double io_read = (proc.io_read_rate < 0 || std::isnan(proc.io_read_rate)) ? 0.0 : proc.io_read_rate;
        double io_write = (proc.io_write_rate < 0 || std::isnan(proc.io_write_rate)) ? 0.0 : proc.io_write_rate;
        double net_rx = (proc.net_rx_rate < 0 || std::isnan(proc.net_rx_rate)) ? 0.0 : proc.net_rx_rate;
        double net_tx = (proc.net_tx_rate < 0 || std::isnan(proc.net_tx_rate)) ? 0.0 : proc.net_tx_rate;

        int color = (proc.state == 'Z') ? 3 : (proc.ppid == 1 && proc.pid != 1) ? 4 : (proc.cpu_usage > 50.0) ? 2 : 1;
        if (line == selected_row)
        {
            wattron(win, A_REVERSE);
        }
        wattron(win, COLOR_PAIR(color));
        mvwprintw(win, line - scroll_offset + 4, 0, "%-5d %-5d %c %6.1f %6.1f %6.1f %6.1f %6llu %6llu %6llu %6llu %4llu %4ld %6llu %5.1f %3ld %3ld %-6s %6.1f %6.1f %-20s",
                  proc.pid, proc.ppid, proc.state, proc.mem_usage, proc.cpu_usage,
                  io_read, io_write, (unsigned long long)(proc.rchar / 1024), (unsigned long long)(proc.wchar / 1024),
                  (unsigned long long)proc.shared_clean, (unsigned long long)proc.private_dirty, (unsigned long long)proc.fd_count, proc.num_threads,
                  (unsigned long long)proc.voluntary_ctxt_switches, proc.process_age, proc.priority, proc.nice,
                  proc.cpus_allowed_list.c_str(), net_rx, net_tx, cmd.c_str());
        wattroff(win, COLOR_PAIR(color));
        if (line == selected_row)
        {
            wattroff(win, A_REVERSE);
        }
        line++;
    }
}

} // namespace DisplayEngine
