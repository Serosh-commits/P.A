#include "ProcessAnalyzer.h"
#include <string>

int main(int argc, char** argv)
{
    if (argc > 1 && std::string(argv[1]) == "--json") {
        ProcessAnalyzer analyzer(false);
        analyzer.printJSON();
    } else {
        ProcessAnalyzer analyzer(true);
        analyzer.run();
    }
    return 0;
}
