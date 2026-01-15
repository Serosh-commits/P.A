#ifndef PROCESS_ANALYZER_H
#define PROCESS_ANALYZER_H

#include "ProcessInfo.h"
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <ncurses.h>

class ProcessAnalyzer
{
private:
    std::vector<ProcessInfo> processes, prev_processes;
    std::map<pid_t, std::vector<pid_t>> process_tree;
    std::map<pid_t, ProcessInfo> process_map;
    uint64_t mem_total = 0, mem_free = 0, prev_total_jiffies = 0, prev_work_jiffies = 0;
    double system_mem_usage = 0.0, system_cpu_usage = 0.0, poll_interval = 1.0, system_uptime = 0.0;
    int num_cores = 0, selected_row = 0, scroll_offset = 0;
    long clk_tck = 0;
    std::ofstream log_file;
    bool logging_enabled = false, tree_view = true, needs_redraw = true, zombie_only = false;
    std::string sort_criterion, status_msg;
    std::vector<Filter> filters;
    WINDOW *win;

    void updateProcessList();
    void handleInput(int ch);

public:
    ProcessAnalyzer(bool ncurses_init = true);
    ~ProcessAnalyzer();

    void printJSON();
    void run();
};

#endif // PROCESS_ANALYZER_H
