#ifndef PROCESS_ANALYZER_H
#define PROCESS_ANALYZER_H

#include "ProcessInfo.h"
#include "SystemUtils.h"
#include <vector>
#include <map>
#include <set>
#include <string>
#include <fstream>
#include <ncurses.h>

class ProcessAnalyzer
{
private:
    std::vector<ProcessInfo> processes;
    std::map<pid_t, ProcessInfo> prev_processes;
    std::map<pid_t, std::vector<pid_t>> process_tree;
    std::map<pid_t, ProcessInfo> process_map;
    std::set<pid_t> tagged_pids;
    uint64_t mem_total = 0, mem_free = 0, prev_total_jiffies = 0, prev_work_jiffies = 0;
    SystemUtils::CPULoadBreakdown cpu_breakdown = {};
    SystemUtils::MemBreakdown mem_breakdown = {};
    double system_mem_usage = 0.0, system_cpu_usage = 0.0, poll_interval = 1.0, system_uptime = 0.0;
    int num_cores = 0, selected_row = 0, scroll_offset = 0, h_scroll_offset = 0;
    long clk_tck = 0;
    std::ofstream log_file;
    bool logging_enabled = false, tree_view = false, needs_redraw = true, zombie_only = false;
    bool filter_mode = false, search_mode = false, sort_inverted = false;
    std::string sort_criterion = "cpu", status_msg, filter_input, search_input;
    std::vector<Filter> filters;
    WINDOW *win;

    void updateProcessList();
    void handleInput(int ch);
    void render();

public:
    ProcessAnalyzer(bool ncurses_init = true);
    ~ProcessAnalyzer();

    void printJSON();
    void run();
};

#endif
