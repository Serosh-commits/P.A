#ifndef FILTER_ENGINE_H
#define FILTER_ENGINE_H

#include "ProcessInfo.h"
#include <vector>
#include <string>

namespace FilterEngine {
    std::vector<Filter> parseFilters(const std::string& input, std::string& status_msg);
    bool matchesFilter(const ProcessInfo& proc, const Filter& filter, std::string& status_msg);
    std::vector<ProcessInfo> filterProcesses(const std::vector<ProcessInfo>& processes, const std::vector<Filter>& filters, std::string& status_msg);
}

#endif // FILTER_ENGINE_H
