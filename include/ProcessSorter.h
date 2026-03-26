#ifndef PROCESS_SORTER_H
#define PROCESS_SORTER_H

#include "ProcessInfo.h"
#include <vector>
#include <string>

namespace ProcessSorter {
    void sortProcesses(std::vector<ProcessInfo>& processes, const std::string& criterion);
}

#endif
