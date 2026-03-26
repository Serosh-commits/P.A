#include "FilterEngine.h"
#include <sstream>
#include <regex>
#include <algorithm>
#include <cmath>

namespace FilterEngine {

std::vector<Filter> parseFilters(const std::string& input, std::string& status_msg)
{
    std::vector<Filter> filters_list;
    if (input.empty())
    {
        return filters_list;
    }
    
    std::regex regex("\\s*(\\w+)\\s*([:<>])\\s*(\\S+)\\s*");
    
    auto words_begin = std::sregex_iterator(input.begin(), input.end(), regex);
    auto words_end = std::sregex_iterator();

    if (words_begin == words_end) {
        status_msg = "Invalid filter format. Use key:val, key>val, etc.";
        return {};
    }

    for (std::sregex_iterator i = words_begin; i != words_end; ++i)
    {
        std::smatch match = *i;
        std::string key = match[1].str();
        std::string op = match[2].str();
        std::string value = match[3].str();
        
        if (key == "pid" || key == "ppid" || key == "state" || key == "cmd" || key == "cpu" || key == "mem" || key == "age")
        {
            Filter f = {key, op, value, 0.0, 0};
            try {
                if (key != "state" && key != "cmd") {
                    f.numeric_val = std::stod(value);
                    f.long_val = std::stol(value);
                }
            } catch (...) {}
            filters_list.push_back(f);
        }
        else
        {
            status_msg = "Invalid filter key: " + key;
            return {};
        }
    }
    return filters_list;
}

bool matchesFilter(const ProcessInfo& proc, const Filter& filter, std::string&)
{
    if (filter.key == "pid")
    {
        if (filter.op == ":" && proc.pid == (pid_t)filter.long_val) return true;
    }
    else if (filter.key == "ppid")
    {
        if (filter.op == ":" && proc.ppid == (pid_t)filter.long_val) return true;
    }
    else if (filter.key == "state")
    {
        if (filter.op == ":" && !filter.value.empty() && proc.state == filter.value[0]) return true;
    }
    else if (filter.key == "cmd")
    {
        std::string cmd = proc.cmd;
        std::string val = filter.value;
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
        if (filter.op == ":" && cmd.find(val) != std::string::npos) return true;
    }
    else if (filter.key == "cpu")
    {
        if (filter.op == ">" && proc.cpu_usage > filter.numeric_val) return true;
        if (filter.op == "<" && proc.cpu_usage < filter.numeric_val) return true;
        if (filter.op == ":" && std::abs(proc.cpu_usage - filter.numeric_val) < 0.1) return true;
    }
    else if (filter.key == "mem")
    {
        if (filter.op == ">" && proc.mem_usage > filter.numeric_val) return true;
        if (filter.op == "<" && proc.mem_usage < filter.numeric_val) return true;
        if (filter.op == ":" && std::abs(proc.mem_usage - filter.numeric_val) < 0.1) return true;
    }
    else if (filter.key == "age")
    {
        if (filter.op == ">" && proc.process_age > filter.numeric_val) return true;
        if (filter.op == "<" && proc.process_age < filter.numeric_val) return true;
        if (filter.op == ":" && std::abs(proc.process_age - filter.numeric_val) < 0.1) return true;
    }
    return false;
}

std::vector<ProcessInfo> filterProcesses(const std::vector<ProcessInfo>& processes, const std::vector<Filter>& filters, std::string& status_msg)
{
    if (filters.empty()) return processes;
    std::vector<ProcessInfo> filtered;
    filtered.reserve(processes.size());
    for (const auto &proc : processes)
    {
        bool pass = true;
        for (const auto &filter : filters)
        {
            if (!matchesFilter(proc, filter, status_msg))
            {
                pass = false;
                break;
            }
        }
        if (pass) filtered.push_back(proc);
    }
    return filtered;
}

}
