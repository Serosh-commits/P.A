#include "ProcessAnalyzer.h"
#include "SystemUtils.h"
#include "FilterEngine.h"
#include "DisplayEngine.h"
#include "ProcessSorter.h"
#include "ProcessLogger.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include <algorithm>

void ProcessAnalyzer::updateProcessList()
{
    SystemUtils::scanProcesses(processes, process_tree, process_map, mem_total, mem_free, 
                               system_mem_usage, system_cpu_usage, prev_total_jiffies, 
                               prev_work_jiffies, prev_processes, num_cores, clk_tck, 
                               system_uptime, poll_interval, status_msg);
    
    prev_processes = processes;

    if (zombie_only)
    {
        std::vector<ProcessInfo> filtered;
        filtered.reserve(processes.size());
        for (const auto &proc : processes)
        {
            if (proc.state == 'Z' || (proc.ppid == 1 && proc.pid != 1))
            {
                filtered.push_back(proc);
            }
        }
        processes = filtered;
    }

    if (!filters.empty())
    {
        processes = FilterEngine::filterProcesses(processes, filters, status_msg);
    }

    if (!sort_criterion.empty())
    {
        ProcessSorter::sortProcesses(processes, sort_criterion);
    }

    if (logging_enabled)
    {
        ProcessLogger::logProcesses(log_file, processes, status_msg);
    }
}

void ProcessAnalyzer::handleInput(int ch)
{
    int max_lines = getmaxy(win) - 3;
    int total_lines = (int)(processes.empty() ? 0 : processes.size());
    status_msg.clear();
    switch (ch)
    {
    case KEY_UP:
        if (selected_row > 0) selected_row--;
        if (selected_row < scroll_offset) scroll_offset--;
        needs_redraw = true;
        break;
    case KEY_DOWN:
        if (selected_row < total_lines - 1) selected_row++;
        if (selected_row >= scroll_offset + max_lines) scroll_offset++;
        needs_redraw = true;
        break;
    case KEY_PPAGE:
        selected_row -= max_lines;
        scroll_offset -= max_lines;
        if (selected_row < 0) selected_row = 0;
        if (scroll_offset < 0) scroll_offset = 0;
        needs_redraw = true;
        break;
    case KEY_NPAGE:
        if (selected_row + max_lines < total_lines) selected_row += max_lines;
        else selected_row = total_lines - 1;
        if (selected_row >= scroll_offset + max_lines) scroll_offset = selected_row - max_lines + 1;
        needs_redraw = true;
        break;
    case KEY_HOME:
        selected_row = 0; scroll_offset = 0;
        needs_redraw = true;
        break;
    case KEY_END:
        selected_row = total_lines - 1;
        if (selected_row >= max_lines) scroll_offset = selected_row - max_lines + 1;
        else scroll_offset = 0;
        needs_redraw = true;
        break;
    case KEY_F(4):
    {
        char input_buf[256] = {0};
        echo();
        mvwprintw(win, 0, 0, "Filter (e.g., pid:1234 cpu>50): ");
        wrefresh(win);
        wgetnstr(win, input_buf, 255);
        noecho();
        std::string s = input_buf;
        zombie_only = false;
        if (s.empty())
        {
            filters.clear();
            status_msg = "Filter cleared";
        }
        else
        {
            filters = FilterEngine::parseFilters(s, status_msg);
            if (filters.empty()) status_msg = status_msg.empty() ? "Invalid filter format" : status_msg;
            else status_msg = "Filter applied: " + s;
        }
        selected_row = 0; scroll_offset = 0;
        needs_redraw = true;
        break;
    }
    case KEY_F(5):
        tree_view = !tree_view;
        selected_row = 0; scroll_offset = 0;
        status_msg = tree_view ? "Tree view enabled" : "List view enabled";
        needs_redraw = true;
        break;
    case KEY_F(6):
        sort_criterion = (sort_criterion == "cpu") ? "mem" : (sort_criterion == "mem") ? "io" : (sort_criterion == "io") ? "net" : "cpu";
        status_msg = "Sort by: " + sort_criterion;
        selected_row = 0; scroll_offset = 0;
        needs_redraw = true;
        break;
    case KEY_F(9):
        if (selected_row >= 0 && selected_row < (int)processes.size())
        {
            auto proc = processes[selected_row];
            std::string prompt;
            bool kill_parent = false;
            if (proc.state == 'Z') { prompt = "Zombie PID=" + std::to_string(proc.pid) + " Kill parent PPID=" + std::to_string(proc.ppid) + "? (y/n): "; kill_parent = true; }
            else if (proc.ppid == 1 && proc.pid != 1) { prompt = "Orphan PID=" + std::to_string(proc.pid) + " Kill? (y/n): "; }
            else { prompt = "Kill PID=" + std::to_string(proc.pid) + "? (y/n): "; }
            mvwprintw(win, 0, 0, "%s", prompt.c_str());
            wrefresh(win);
            char c = getch();
            if (c == 'y' || c == 'Y')
            {
                pid_t pid_to_kill = kill_parent ? proc.ppid : proc.pid;
                if (kill(pid_to_kill, SIGTERM) == 0) status_msg = "SIGTERM sent to PID " + std::to_string(pid_to_kill);
                else status_msg = "Error: Failed to kill PID " + std::to_string(pid_to_kill);
            }
            else status_msg = "Kill cancelled";
        }
        else status_msg = "No process selected";
        needs_redraw = true;
        break;
    case 'z':
    case 'Z':
        zombie_only = !zombie_only;
        filters.clear();
        status_msg = zombie_only ? "Showing zombies and orphans only" : "Showing all processes";
        selected_row = 0; scroll_offset = 0;
        needs_redraw = true;
        break;
    case 'l':
    case 'L':
        logging_enabled = !logging_enabled;
        if (!logging_enabled && log_file.is_open()) log_file.close();
        status_msg = logging_enabled ? "Logging enabled" : "Logging disabled";
        needs_redraw = true;
        break;
    case 'q':
    case 'Q':
        endwin();
        if (log_file.is_open()) log_file.close();
        exit(0);
    case KEY_RESIZE:
        endwin(); refresh(); werase(win);
        needs_redraw = true;
        break;
    }
}

void ProcessAnalyzer::printJSON()
{
    updateProcessList();
    std::cout << "{\n";
    std::cout << "  \"system\": {\n";
    std::cout << "    \"cpu_usage\": " << system_cpu_usage << ",\n";
    std::cout << "    \"mem_usage\": " << system_mem_usage << ",\n";
    std::cout << "    \"mem_total\": " << mem_total << ",\n";
    std::cout << "    \"mem_free\": " << mem_free << ",\n";
    std::cout << "    \"uptime\": " << system_uptime << ",\n";
    std::cout << "    \"num_cores\": " << num_cores << "\n";
    std::cout << "  },\n";
    std::cout << "  \"processes\": [\n";
    for (size_t i = 0; i < processes.size(); ++i)
    {
        const auto &p = processes[i];
        std::cout << "    {\n";
        std::cout << "      \"pid\": " << p.pid << ",\n";
        std::cout << "      \"ppid\": " << p.ppid << ",\n";
        std::cout << "      \"state\": \"" << p.state << "\",\n";
        std::cout << "      \"cmd\": \"" << p.cmd << "\",\n";
        std::cout << "      \"cpu_usage\": " << p.cpu_usage << ",\n";
        std::cout << "      \"mem_usage\": " << p.mem_usage << ",\n";
        std::cout << "      \"rss\": " << p.rss << ",\n";
        std::cout << "      \"threads\": " << p.num_threads << ",\n";
        std::cout << "      \"io_read\": " << p.io_read_rate << ",\n";
        std::cout << "      \"io_write\": " << p.io_write_rate << ",\n";
        std::cout << "      \"net_rx\": " << p.net_rx_rate << ",\n";
        std::cout << "      \"net_tx\": " << p.net_tx_rate << ",\n";
        std::cout << "      \"age\": " << p.process_age << "\n";
        std::cout << "    }" << (i == processes.size() - 1 ? "" : ",") << "\n";
    }
    std::cout << "  ]\n";
    std::cout << "}\n";
}

ProcessAnalyzer::ProcessAnalyzer(bool ncurses_init)
{
    clk_tck = sysconf(_SC_CLK_TCK);
    system_uptime = SystemUtils::getUptime();
    if (ncurses_init)
    {
        initscr(); start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_RED, COLOR_BLACK);
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);
        init_pair(4, COLOR_BLUE, COLOR_BLACK);
        cbreak(); noecho(); keypad(stdscr, TRUE);
        curs_set(0); timeout(50);
        win = newwin(0, 0, 0, 0);
    }
    else win = nullptr;
}

ProcessAnalyzer::~ProcessAnalyzer() { if (log_file.is_open()) log_file.close(); if (win) endwin(); }

void ProcessAnalyzer::run()
{
    while (true)
    {
        system_uptime = SystemUtils::getUptime();
        updateProcessList();
        
        if (needs_redraw)
        {
            werase(win);
            DisplayEngine::displayHeader(win, mem_total, mem_free, system_cpu_usage, system_mem_usage, 
                                          system_uptime, num_cores, filters, logging_enabled, 
                                          sort_criterion, status_msg);
            wattron(win, A_BOLD);
            mvwprintw(win, 3, 0, "%-5s %-5s %s %6s %6s %6s %6s %6s %6s %6s %6s %4s %4s %6s %5s %3s %3s %-6s %6s %6s %-20s",
                      "PID", "PPID", "S", "Mem%", "CPU%", "IO R", "IO W", "RChar", "WChar", "Shared", "Priv", "FD", "Thrd", "CtxtSw", "Age", "Pri", "Nice", "CPUs", "Net R", "Net W", "Cmd");
            wattroff(win, A_BOLD);
        }

        int max_lines = getmaxy(win) - 5;
        if (processes.empty()) { if (needs_redraw) mvwprintw(win, 4, 0, "No processes to display"); }
        else if (tree_view) { int line = 0; DisplayEngine::displayTree(win, 1, 0, line, max_lines, scroll_offset, selected_row, process_map, process_tree); }
        else { DisplayEngine::displayProcesses(win, max_lines, scroll_offset, selected_row, processes); }
        
        wrefresh(win);
        needs_redraw = false;

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < poll_interval)
        {
            int ch = getch();
            if (ch != ERR)
            {
                handleInput(ch);
                if (needs_redraw)
                {
                    werase(win);
                    DisplayEngine::displayHeader(win, mem_total, mem_free, system_cpu_usage, system_mem_usage, 
                                                  system_uptime, num_cores, filters, logging_enabled, 
                                                  sort_criterion, status_msg);
                    wattron(win, A_BOLD);
                    mvwprintw(win, 3, 0, "%-5s %-5s %s %6s %6s %6s %6s %6s %6s %6s %6s %4s %4s %6s %5s %3s %3s %-6s %6s %6s %-20s",
                              "PID", "PPID", "S", "Mem%", "CPU%", "IO R", "IO W", "RChar", "WChar", "Shared", "Priv", "FD", "Thrd", "CtxtSw", "Age", "Pri", "Nice", "CPUs", "Net R", "Net W", "Cmd");
                    wattroff(win, A_BOLD);
                }
                if (processes.empty()) { if (needs_redraw) mvwprintw(win, 5, 0, "No processes to display"); }
                else if (tree_view) { int line = 0; DisplayEngine::displayTree(win, 1, 0, line, max_lines, scroll_offset, selected_row, process_map, process_tree); }
                else { DisplayEngine::displayProcesses(win, max_lines, scroll_offset, selected_row, processes); }
                wrefresh(win);
                needs_redraw = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
