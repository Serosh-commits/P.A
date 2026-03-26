#ifndef PROCESS_LOGGER_H
#define PROCESS_LOGGER_H

#include "ProcessInfo.h"
#include <vector>
#include <fstream>
#include <string>

namespace ProcessLogger {
    void logProcesses(std::ofstream& log_file, const std::vector<ProcessInfo>& processes, std::string& status_msg);
}

#endif
