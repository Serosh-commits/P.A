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
#include <cstring>
#include <cctype>

static void drawFunctionBar(WINDOW* win, bool filter_mode, const std::string& filter_input)
{
    int width = getmaxx(win);
    int y = getmaxy(win) - 1;

    wattron(win, A_REVERSE);
    mvwhline(win, y, 0, ' ', width);

    if (filter_mode) {
        mvwprintw(win, y, 0, " Filter: %s", filter_input.c_str());
        wattroff(win, A_REVERSE);
        return;
    }

    const char* keys[]   = {"F1", "F3", "F4", "F5", "F6", "F9", "F10", NULL};
    const char* labels[] = {"Help ", "Search", "Filter", "Tree  ", "SortBy", "Kill  ", "Quit  ", NULL};

    int x = 0;
    for (int i = 0; keys[i] != NULL; i++) {
        wattrset(win, COLOR_PAIR(4) | A_BOLD);
        mvwaddstr(win, y, x, keys[i]);
        x += strlen(keys[i]);
        wattrset(win, A_REVERSE);
        waddstr(win, labels[i]);
        x += strlen(labels[i]);
        waddch(win, ' ');
        x++;
    }
    wattrset(win, A_NORMAL);
}

static void showHelp(WINDOW* win)
{
    werase(win);
    int line = 0;
    wattron(win, A_BOLD | COLOR_PAIR(1));
    mvwprintw(win, line++, 0, "Process Analyzer - Keyboard Shortcuts");
    wattroff(win, A_BOLD);
    line++;
    wattron(win, COLOR_PAIR(1));
    mvwprintw(win, line++, 0, " Arrows      : scroll/navigate process list");
    mvwprintw(win, line++, 0, " Left/Right  : horizontal scroll columns");
    mvwprintw(win, line++, 0, " PgUp/PgDn   : page up/down");
    mvwprintw(win, line++, 0, " Home/End    : jump to top/bottom");
    line++;
    mvwprintw(win, line++, 0, " F1 h ?      : show this help screen");
    mvwprintw(win, line++, 0, " F3 /        : incremental search by name");
    mvwprintw(win, line++, 0, " F4 \\        : incremental filter by name");
    mvwprintw(win, line++, 0, " F5 t        : toggle tree/list view");
    mvwprintw(win, line++, 0, " F6 > .      : cycle sort (CPU/Mem/IO/Net)");
    mvwprintw(win, line++, 0, " F9 k        : kill selected process");
    mvwprintw(win, line++, 0, " F10 q       : quit");
    line++;
    mvwprintw(win, line++, 0, " z           : show zombies/orphans only");
    mvwprintw(win, line++, 0, " x           : purge all zombie processes");
    mvwprintw(win, line++, 0, " l           : toggle logging to file");
    mvwprintw(win, line++, 0, " I           : invert sort order");
    mvwprintw(win, line++, 0, " M           : sort by memory");
    mvwprintw(win, line++, 0, " P           : sort by CPU");
    mvwprintw(win, line++, 0, " N           : sort by PID");
    mvwprintw(win, line++, 0, " Space       : tag/untag process");
    mvwprintw(win, line++, 0, " U           : untag all");
    line++;
    wattron(win, A_BOLD);
    mvwprintw(win, line++, 0, " Process state: R=running S=sleeping Z=zombie D=disk T=stopped");
    wattroff(win, A_BOLD);
    line++;
    mvwprintw(win, line++, 0, "Press any key to return.");
    wattroff(win, COLOR_PAIR(1));
    wrefresh(win);
    timeout(-1);
    wgetch(win);
    timeout(0);
}

void ProcessAnalyzer::updateProcessList()
{
    SystemUtils::scanProcesses(processes, process_tree, process_map, mem_total, mem_free,
                               system_mem_usage, system_cpu_usage, prev_total_jiffies,
                               prev_work_jiffies, prev_processes, num_cores, clk_tck,
                               system_uptime, poll_interval, status_msg, cpu_breakdown, mem_breakdown);

    for (const auto &p : processes) prev_processes[p.pid] = p;

    if (zombie_only) {
        std::vector<ProcessInfo> filtered;
        for (const auto &proc : processes)
            if (proc.state == 'Z' || (proc.ppid == 1 && proc.pid != 1))
                filtered.push_back(proc);
        processes = filtered;
    }

    if (!filter_input.empty()) {
        std::vector<ProcessInfo> filtered;
        std::string lower_filter = filter_input;
        std::transform(lower_filter.begin(), lower_filter.end(), lower_filter.begin(), ::tolower);
        for (const auto &proc : processes) {
            std::string lower_cmd = proc.cmd;
            std::transform(lower_cmd.begin(), lower_cmd.end(), lower_cmd.begin(), ::tolower);
            if (lower_cmd.find(lower_filter) != std::string::npos)
                filtered.push_back(proc);
        }
        processes = filtered;
    }

    if (!sort_criterion.empty()) ProcessSorter::sortProcesses(processes, sort_criterion);
    if (sort_inverted) std::reverse(processes.begin(), processes.end());
    if (logging_enabled) ProcessLogger::logProcesses(log_file, processes, status_msg);
}

void ProcessAnalyzer::handleInput(int ch)
{
    int max_lines = getmaxy(win) - 6;
    int total_lines = (int)processes.size();

    if (filter_mode) {
        if (ch == 27 || ch == KEY_F(4)) {
            if (ch == 27) { filter_input.clear(); }
            filter_mode = false;
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            if (!filter_input.empty()) filter_input.pop_back();
            if (filter_input.empty()) filter_mode = false;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            filter_mode = false;
        } else if (ch > 0 && ch < 256 && isprint(ch)) {
            filter_input += (char)ch;
        }
        selected_row = 0; scroll_offset = 0;
        needs_redraw = true;
        return;
    }

    if (search_mode) {
        if (ch == 27 || ch == KEY_F(3)) { search_mode = false; search_input.clear(); }
        else if (ch == KEY_BACKSPACE || ch == 127) {
            if (!search_input.empty()) search_input.pop_back();
            if (search_input.empty()) search_mode = false;
        }
        else if (ch == '\n' || ch == KEY_ENTER) { search_mode = false; }
        else if (ch > 0 && ch < 256 && isprint(ch)) {
            search_input += (char)ch;
            std::string lower_search = search_input;
            std::transform(lower_search.begin(), lower_search.end(), lower_search.begin(), ::tolower);
            for (int i = 0; i < total_lines; i++) {
                std::string lower_cmd = processes[i].cmd;
                std::transform(lower_cmd.begin(), lower_cmd.end(), lower_cmd.begin(), ::tolower);
                if (lower_cmd.find(lower_search) != std::string::npos) {
                    selected_row = i;
                    if (selected_row >= scroll_offset + max_lines) scroll_offset = selected_row - max_lines + 1;
                    if (selected_row < scroll_offset) scroll_offset = selected_row;
                    break;
                }
            }
        }
        needs_redraw = true;
        return;
    }

    status_msg.clear();
    switch (ch)
    {
    case KEY_UP:
        if (selected_row > 0) selected_row--;
        if (selected_row < scroll_offset) scroll_offset--;
        needs_redraw = true; break;
    case KEY_DOWN:
        if (selected_row < total_lines - 1) selected_row++;
        if (selected_row >= scroll_offset + max_lines) scroll_offset++;
        needs_redraw = true; break;
    case KEY_LEFT:
        if (h_scroll_offset > 0) h_scroll_offset -= 4;
        if (h_scroll_offset < 0) h_scroll_offset = 0;
        needs_redraw = true; break;
    case KEY_RIGHT:
        h_scroll_offset += 4;
        needs_redraw = true; break;
    case KEY_PPAGE:
        selected_row -= max_lines; scroll_offset -= max_lines;
        if (selected_row < 0) selected_row = 0;
        if (scroll_offset < 0) scroll_offset = 0;
        needs_redraw = true; break;
    case KEY_NPAGE:
        if (selected_row + max_lines < total_lines) selected_row += max_lines;
        else selected_row = total_lines - 1;
        if (selected_row >= scroll_offset + max_lines) scroll_offset = selected_row - max_lines + 1;
        needs_redraw = true; break;
    case KEY_HOME:
        selected_row = 0; scroll_offset = 0;
        needs_redraw = true; break;
    case KEY_END:
        selected_row = total_lines - 1;
        if (selected_row >= max_lines) scroll_offset = selected_row - max_lines + 1;
        else scroll_offset = 0;
        needs_redraw = true; break;

    case KEY_F(1): case 'h': case '?':
        showHelp(win); needs_redraw = true; break;

    case KEY_F(3): case '/':
        search_mode = true; search_input.clear();
        status_msg = "Search: type to find, Esc to cancel";
        needs_redraw = true; break;

    case KEY_F(4): case '\\':
        filter_mode = true;
        status_msg = "Filter: type to filter, Enter to confirm, Esc to clear";
        needs_redraw = true; break;

    case KEY_F(5): case 't':
        tree_view = !tree_view; selected_row = 0; scroll_offset = 0;
        status_msg = tree_view ? "Tree view" : "List view";
        needs_redraw = true; break;

    // Here be dragons.
    case KEY_F(6): case '>': case '.':
        sort_criterion = (sort_criterion == "cpu") ? "mem" : (sort_criterion == "mem") ? "io" : (sort_criterion == "io") ? "net" : "cpu";
        status_msg = "Sort: " + sort_criterion;
        selected_row = 0; scroll_offset = 0;
        needs_redraw = true; break;

    case KEY_F(9): case 'k':
        if (selected_row >= 0 && selected_row < (int)processes.size()) {
            auto proc = processes[selected_row];
            std::string prompt;
            bool kill_parent = false;
            if (proc.state == 'Z') {
                prompt = "Zombie PID=" + std::to_string(proc.pid) + " Kill parent PPID=" + std::to_string(proc.ppid) + "? (y/n): ";
                kill_parent = true;
            } else {
                prompt = "Send SIGTERM to PID=" + std::to_string(proc.pid) + " [" + proc.cmd + "]? (y/n): ";
            }
            mvwprintw(win, getmaxy(win)-1, 0, "%s", prompt.c_str());
            wclrtoeol(win); wrefresh(win);
            timeout(-1); char c = wgetch(win); timeout(0);
            if (c == 'y' || c == 'Y') {
                pid_t target = kill_parent ? proc.ppid : proc.pid;
                if (kill(target, SIGTERM) == 0) status_msg = "SIGTERM -> PID " + std::to_string(target);
                else status_msg = "Failed to kill PID " + std::to_string(target);
            } else status_msg = "Cancelled";
        }
        needs_redraw = true; break;

    case KEY_F(10): case 'q': endwin(); exit(0);

    case 'M': sort_criterion = "mem"; status_msg = "Sort: mem"; needs_redraw = true; break;
    case 'P': sort_criterion = "cpu"; status_msg = "Sort: cpu"; needs_redraw = true; break;
    case 'N': sort_criterion = ""; sort_inverted = false; status_msg = "Sort: PID (default)"; needs_redraw = true; break;
    case 'I': sort_inverted = !sort_inverted; status_msg = sort_inverted ? "Sort inverted" : "Sort normal"; needs_redraw = true; break;

    case 'z':
        zombie_only = !zombie_only; filter_input.clear();
        status_msg = zombie_only ? "Zombies/orphans only" : "All processes";
        selected_row = 0; scroll_offset = 0;
        needs_redraw = true; break;

    case 'x': {
        int killed = 0;
        for (const auto &p : processes) {
            if (p.state == 'Z') {
                if (kill(p.ppid, SIGCHLD) == 0) killed++;
                else kill(p.ppid, SIGTERM);
            }
        }
        status_msg = "Purged " + std::to_string(killed) + " zombie(s)";
        needs_redraw = true; break;
    }

    case 'l':
        logging_enabled = !logging_enabled;
        if (!logging_enabled && log_file.is_open()) log_file.close();
        status_msg = logging_enabled ? "Logging ON" : "Logging OFF";
        needs_redraw = true; break;

    case ' ':
        if (selected_row >= 0 && selected_row < (int)processes.size()) {
            tagged_pids.insert(processes[selected_row].pid);
            if (selected_row < total_lines - 1) selected_row++;
            if (selected_row >= scroll_offset + max_lines) scroll_offset++;
        }
        needs_redraw = true; break;

    case 'U':
        tagged_pids.clear(); status_msg = "Untagged all";
        needs_redraw = true; break;

    case KEY_RESIZE:
        endwin(); refresh(); werase(win);
        needs_redraw = true; break;
    }
}

void ProcessAnalyzer::render()
{
    werase(win);
    DisplayEngine::displayHeader(win, mem_total, mem_free, system_cpu_usage, system_mem_usage,
                                  system_uptime, num_cores, filters, logging_enabled,
                                  sort_criterion, status_msg, cpu_breakdown, mem_breakdown);

    int width = getmaxx(win);
    int cmd_w = std::min(40, std::max(15, width - 35));
    wattrset(win, COLOR_PAIR(6) | A_BOLD | A_UNDERLINE);
    char hdr[1024];
    std::string cmd_hdr = "Command";
    if ((int)cmd_hdr.size() < cmd_w) cmd_hdr += std::string(cmd_w - cmd_hdr.size(), ' ');
    else cmd_hdr = cmd_hdr.substr(0, cmd_w);
    std::snprintf(hdr, sizeof(hdr), "%5s %5s %1s %5s %5s %s %6s %6s %6s %6s %6s %6s %5s %4s %6s %5s %3s %3s %6s %6s",
              "PID", "PPID", "S", "CPU%", "MEM%", cmd_hdr.c_str(),
              "IO_R", "IO_W", "RChr", "WChr", "ShrCl", "PrvDr", "FD", "Thr", "CtxSw", "Age", "Pri", "Ni", "NetR", "NetW");
    int hlen = (int)std::strlen(hdr);
    if (hlen > h_scroll_offset) mvwaddnstr(win, 4, 0, hdr + h_scroll_offset, width);
    wattrset(win, A_NORMAL);

    int max_lines = getmaxy(win) - 6;
    if (processes.empty()) {
        mvwprintw(win, 6, 0, "No processes to display");
    } else if (tree_view) {
        int line = 0;
        auto roots = process_tree.find(0);
        if (roots != process_tree.end()) {
            for (auto pid : roots->second)
                DisplayEngine::displayTree(win, pid, 0, line, max_lines, scroll_offset, h_scroll_offset, selected_row, process_map, process_tree);
        } else {
            DisplayEngine::displayTree(win, 1, 0, line, max_lines, scroll_offset, h_scroll_offset, selected_row, process_map, process_tree);
        }
    } else {
        DisplayEngine::displayProcesses(win, max_lines, scroll_offset, h_scroll_offset, selected_row, processes);
    }

    // Here be dragons.
    drawFunctionBar(win, filter_mode || search_mode,
                    filter_mode ? ("Filter: " + filter_input) : ("Search: " + search_input));

    wrefresh(win);
    needs_redraw = false;
}

void ProcessAnalyzer::run()
{
    while (true)
    {
        system_uptime = SystemUtils::getUptime();
        updateProcessList();
        render();

        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < poll_interval)
        {
            int ch;
            bool handled = false;
            while ((ch = getch()) != ERR) { handleInput(ch); handled = true; }
            if (handled && needs_redraw) render();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }
}

ProcessAnalyzer::ProcessAnalyzer(bool ncurses_init)
{
    clk_tck = sysconf(_SC_CLK_TCK);
    num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    system_uptime = SystemUtils::getUptime();
    if (ncurses_init)
    {
        initscr(); start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_RED, COLOR_BLACK);
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);
        init_pair(4, COLOR_CYAN, COLOR_BLACK);
        init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(6, COLOR_WHITE, COLOR_BLACK);
        init_pair(7, COLOR_BLUE, COLOR_BLACK);
        cbreak(); noecho(); keypad(stdscr, TRUE);
        curs_set(0); timeout(0);
        win = newwin(0, 0, 0, 0);
        keypad(win, TRUE);
    }
    else win = nullptr;
}

ProcessAnalyzer::~ProcessAnalyzer() {
    if (log_file.is_open()) log_file.close();
    if (win) { delwin(win); endwin(); }
}

void ProcessAnalyzer::printJSON()
{
    system_uptime = SystemUtils::getUptime();
    updateProcessList();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    system_uptime = SystemUtils::getUptime();
    updateProcessList();

    std::cout << "{\n  \"system\": {\n"
              << "    \"cpu_usage\": " << system_cpu_usage << ",\n"
              << "    \"mem_usage\": " << system_mem_usage << ",\n"
              << "    \"mem_total\": " << mem_total << ",\n"
              << "    \"mem_free\": " << mem_free << ",\n"
              << "    \"uptime\": " << system_uptime << ",\n"
              << "    \"num_cores\": " << num_cores << "\n  },\n";
    std::cout << "  \"processes\": [\n";
    for (size_t i = 0; i < processes.size(); ++i) {
        const auto &p = processes[i];
        std::cout << "    {\"pid\":" << p.pid << ",\"ppid\":" << p.ppid
                  << ",\"state\":\"" << p.state << "\",\"cmd\":\"" << p.cmd
                  << "\",\"cpu\":" << p.cpu_usage << ",\"mem\":" << p.mem_usage
                  << ",\"rss\":" << p.rss << ",\"threads\":" << p.num_threads
                  << ",\"io_r\":" << p.io_read_rate << ",\"io_w\":" << p.io_write_rate
                  << ",\"net_rx\":" << p.net_rx_rate << ",\"net_tx\":" << p.net_tx_rate
                  << ",\"age\":" << p.process_age << "}"
                  << (i < processes.size()-1 ? "," : "") << "\n";
    }
    std::cout << "  ]\n}\n";
}
