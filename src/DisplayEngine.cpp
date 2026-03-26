#include "DisplayEngine.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace DisplayEngine {

static char buf[2048];

static inline double sane(double v) { return (v < 0 || std::isnan(v)) ? 0.0 : v; }

static std::string fitstr(const std::string& s, int w) {
    if (w <= 0) return "";
    if ((int)s.size() >= w) return s.substr(0, w);
    return s + std::string(w - s.size(), ' ');
}

void displayHeader(WINDOW* win, uint64_t, uint64_t,
                    double system_cpu_usage, double,
                    double system_uptime, int num_cores,
                    const std::vector<Filter>& /*filters*/, bool logging_enabled,
                    const std::string& sort_criterion, const std::string& status_msg,
                    const SystemUtils::CPULoadBreakdown& b,
                    const SystemUtils::MemBreakdown& m)
{
    int width = getmaxx(win);
    double load[3] = {0,0,0};
    getloadavg(load, 3);

    int bar_w = (width - 25) / 2;
    if (bar_w < 5)   bar_w = 5;
    if (bar_w > 100) bar_w = 100;

    // Only God and I knew what this did. Now only God knows.
    mvwaddstr(win, 0, 0, "CPU[");
    int x_fill = 0;
    auto draw_cpu = [&](double pct, int cp_idx) {
        int count = (int)(pct * bar_w / 100.0);
        wattrset(win, COLOR_PAIR(cp_idx) | A_BOLD);
        for(int i=0; i<count && x_fill<bar_w; i++, x_fill++) waddch(win, '|');
    };
    draw_cpu(b.user, 1);
    draw_cpu(b.nice, 7);
    draw_cpu(b.sys,  2);
    draw_cpu(b.irq + b.softirq, 3);
    wattrset(win, COLOR_PAIR(6));
    while(x_fill < bar_w) { waddch(win, ' '); x_fill++; }
    wprintw(win, "%5.1f%%]", system_cpu_usage);
    
    if (width > 65)
        mvwprintw(win, 0, width - 28, "Load: %.2f %.2f %.2f", load[0], load[1], load[2]);

    mvwaddstr(win, 1, 0, "Mem[");
    int m_fill = 0;
    auto draw_mem = [&](uint64_t kb, int cp_idx) {
        int count = (int)((double)kb * bar_w / (double)m.total);
        wattrset(win, COLOR_PAIR(cp_idx) | A_BOLD);
        for(int i=0; i<count && m_fill<bar_w; i++, m_fill++) waddch(win, '|');
    };
    draw_mem(m.shorthand_used, 1);
    draw_mem(m.buffers, 7);
    draw_mem(m.cached + m.s_reclaimable, 3);
    wattrset(win, COLOR_PAIR(6));
    while(m_fill < bar_w) { waddch(win, ' '); m_fill++; }
    
    double used_gb = (double)(m.total - m.free) / 1024.0 / 1024.0;
    double total_gb = (double)m.total / 1024.0 / 1024.0;
    wprintw(win, "%.1f/%.1fG]", used_gb, total_gb);

    if (width > 65) {
        int ud = (int)(system_uptime/86400), uh = ((int)system_uptime%86400)/3600, um = ((int)system_uptime%3600)/60;
        mvwprintw(win, 1, width - 28, "Up: %dd%02dh%02dm  Cores: %d", ud, uh, um, num_cores);
    }

    wattrset(win, COLOR_PAIR(6));
    std::snprintf(buf, sizeof(buf), " Sort: %s | Log: %s",
                  sort_criterion.empty() ? "PID" : sort_criterion.c_str(),
                  logging_enabled ? "ON" : "OFF");
    mvwaddnstr(win, 2, 0, buf, width);
    if (!status_msg.empty()) {
        wattrset(win, COLOR_PAIR(3) | A_BOLD);
        mvwaddnstr(win, 3, 0, status_msg.c_str(), width);
    }
    wattrset(win, A_NORMAL);
}

static void formatProcessLine(const ProcessInfo& p, const std::string& display_cmd, int cmd_w)
{
    std::string cmd_fixed = fitstr(display_cmd, cmd_w);
    std::snprintf(buf, sizeof(buf),
        "%5d %5d %c %5.1f %5.1f %s %6.1f %6.1f %6d %6d %6llu %6llu %5llu %4ld %6llu %5.1f %3ld %3ld %6.1f %6.1f",
        (int)p.pid, (int)p.ppid, p.state,
        sane(p.cpu_usage), sane(p.mem_usage),
        cmd_fixed.c_str(),
        sane(p.io_read_rate), sane(p.io_write_rate),
        (int)(p.rchar/1024), (int)(p.wchar/1024),
        (unsigned long long)p.shared_clean, (unsigned long long)p.private_dirty,
        (unsigned long long)p.fd_count, p.num_threads,
        (unsigned long long)p.voluntary_ctxt_switches,
        sane(p.process_age), p.priority, p.nice,
        sane(p.net_rx_rate), sane(p.net_tx_rate));
}

static int getAttrForState(const ProcessInfo& p, int selected_row, int line) {
    if (line == selected_row) return A_REVERSE;
    char st = p.state;
    if (st == 'R') return COLOR_PAIR(1) | A_BOLD;
    if (st == 'Z') return COLOR_PAIR(2) | A_BOLD;
    if (st == 'D') return COLOR_PAIR(2);
    if (st == 'T') return COLOR_PAIR(5);
    if (st == 'I') return COLOR_PAIR(6) | A_DIM;
    if (p.cpu_usage > 50.0) return COLOR_PAIR(3);
    if (p.ppid == 1 && p.pid != 1) return COLOR_PAIR(4);
    return COLOR_PAIR(6);
}

static void renderLine(WINDOW* win, int screen_y, int h_scroll_offset, int width, int attr) {
    wattrset(win, attr);
    int len = (int)std::strlen(buf);
    if (h_scroll_offset < len)
        mvwaddnstr(win, screen_y, 0, buf + h_scroll_offset, width);
    else
        mvwhline(win, screen_y, 0, ' ', width);
    wattrset(win, A_NORMAL);
}

void displayTree(WINDOW* win, pid_t pid, int depth, int &line, int max_lines,
                    int scroll_offset, int h_scroll_offset, int selected_row,
                    const std::map<pid_t, ProcessInfo>& process_map,
                    const std::map<pid_t, std::vector<pid_t>>& process_tree)
{
    auto it = process_map.find(pid);
    if (it == process_map.end()) return;

    if (line >= scroll_offset && line < max_lines + scroll_offset) {
        int width = getmaxx(win);
        int cmd_w = std::min(40, std::max(15, width - 35));

        std::string indent;
        for (int d = 0; d < depth; d++)
            indent += (d == depth - 1) ? " |- " : "    ";
        std::string display_cmd = indent + it->second.cmd;

        formatProcessLine(it->second, display_cmd, cmd_w);
        int attr = getAttrForState(it->second, selected_row, line);
        renderLine(win, line - scroll_offset + 5, h_scroll_offset, width, attr);
    }
    line++;
    auto children = process_tree.find(pid);
    if (children != process_tree.end())
        for (const auto &child : children->second)
            displayTree(win, child, depth + 1, line, max_lines, scroll_offset, h_scroll_offset, selected_row, process_map, process_tree);
}

void displayProcesses(WINDOW* win, int max_lines, int scroll_offset, int h_scroll_offset,
                        int selected_row, const std::vector<ProcessInfo>& processes)
{
    int width = getmaxx(win);
    int cmd_w = std::min(40, std::max(15, width - 35));
    int line = 0;

    for (const auto &proc : processes) {
        if (line >= max_lines + scroll_offset) break;
        if (line < scroll_offset) { line++; continue; }

        formatProcessLine(proc, proc.cmd, cmd_w);
        int attr = getAttrForState(proc, selected_row, line);
        renderLine(win, line - scroll_offset + 5, h_scroll_offset, width, attr);
        line++;
    }
}

}
