#ifndef DISPLAY_ENGINE_H
#define DISPLAY_ENGINE_H

#include "ProcessInfo.h"
#include "SystemUtils.h"
#include <vector>
#include <map>
#include <string>
#include <ncurses.h>

namespace DisplayEngine {
    void displayHeader(WINDOW* win, uint64_t mem_total, uint64_t mem_free, 
                        double system_cpu_usage, double system_mem_usage, 
                        double system_uptime, int num_cores, 
                        const std::vector<Filter>& filters, bool logging_enabled, 
                        const std::string& sort_criterion, const std::string& status_msg,
                        const SystemUtils::CPULoadBreakdown& cpu_breakdown,
                        const SystemUtils::MemBreakdown& mem_breakdown);
    
    void displayTree(WINDOW* win, pid_t pid, int depth, int &line, int max_lines, 
                        int scroll_offset, int h_scroll_offset, int selected_row, 
                        const std::map<pid_t, ProcessInfo>& process_map, 
                        const std::map<pid_t, std::vector<pid_t>>& process_tree);
    
    void displayProcesses(WINDOW* win, int max_lines, int scroll_offset, int h_scroll_offset, 
                            int selected_row, const std::vector<ProcessInfo>& processes);
}

#endif
