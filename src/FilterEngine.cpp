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
    std::istringstream iss(input);
    std::string token;
    std::regex regex("(\\w+)([:<>])(.*)");
    while (iss >> token)
    {
        std::smatch match;
        if (std::regex_match(token, match, regex))
        {
            std::string key = match[1].str();
            std::string op = match[2].str();
            std::string value = match[3].str();
            if (key == "pid" || key == "ppid" || key == "state" || key == "cmd" || key == "cpu" || key == "mem" || key == "age")
            {
                filters_list.push_back({key, op, value});
            }
            else
            {
                status_msg = "Invalid filter key: " + key;
                return {};
            }
        }
        else
        {
            status_msg = "Invalid filter format: " + token;
            return {};
        }
    }
    return filters_list;
}

bool matchesFilter(const ProcessInfo& proc, const Filter& filter, std::string& status_msg)
{
    if (filter.key == "pid")
    {
        try
        {
            pid_t val = std::stoi(filter.value);
            if (filter.op == ":" && proc.pid == val)
            {
                return true;
            }
        }
        catch (...)
        {
            status_msg = "Invalid PID: " + filter.value;
            return false;
        }
    }
    else if (filter.key == "ppid")
    {
        try
        {
            pid_t val = std::stoi(filter.value);
            if (filter.op == ":" && proc.ppid == val)
            {
                return true;
            }
        }
        catch (...)
        {
            status_msg = "Invalid PPID: " + filter.value;
            return false;
        }
    }
    else if (filter.key == "state")
    {
        if (filter.op == ":" && proc.state == filter.value[0])
        {
            return true;
        }
    }
    else if (filter.key == "cmd")
    {
        std::string cmd = proc.cmd;
        std::string val = filter.value;
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
        if (filter.op == ":" && cmd.find(val) != std::string::npos)
        {
            return true;
        }
    }
    else if (filter.key == "cpu")
    {
        try
        {
            double val = std::stod(filter.value);
            if (filter.op == ">" && proc.cpu_usage > val)
            {
                return true;
            }
            if (filter.op == "<" && proc.cpu_usage < val)
            {
                return true;
            }
            if (filter.op == ":" && std::abs(proc.cpu_usage - val) < 0.01)
            {
                return true;
            }
        }
        catch (...)
        {
            status_msg = "Invalid CPU value: " + filter.value;
            return false;
        }
    }
    else if (filter.key == "mem")
    {
        try
        {
            double val = std::stod(filter.value);
            if (filter.op == ">" && proc.mem_usage > val)
            {
                return true;
            }
            if (filter.op == "<" && proc.mem_usage < val)
            {
                return true;
            }
            if (filter.op == ":" && std::abs(proc.mem_usage - val) < 0.01)
            {
                return true;
            }
        }
        catch (...)
        {
            status_msg = "Invalid Mem value: " + filter.value;
            return false;
        }
    }
    else if (filter.key == "age")
    {
        try
        {
            double val = std::stod(filter.value);
            if (filter.op == ">" && proc.process_age > val)
            {
                return true;
            }
            if (filter.op == "<" && proc.process_age < val)
            {
                return true;
            }
            if (filter.op == ":" && std::abs(proc.process_age - val) < 0.01)
            {
                return true;
            }
        }
        catch (...)
        {
            status_msg = "Invalid Age value: " + filter.value;
            return false;
        }
    }
    return false;
}

std::vector<ProcessInfo> filterProcesses(const std::vector<ProcessInfo>& processes, const std::vector<Filter>& filters, std::string& status_msg)
{
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
        if (pass)
        {
            filtered.push_back(proc);
        }
    }
    return filtered;
}

} // namespace FilterEngine
