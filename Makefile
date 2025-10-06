CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LIBS = -lncurses
TARGET = process_analyzer
SOURCE = process_analyzer_fixed.cpp

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

clean:
	rm -f $(TARGET)

install-deps:
	sudo apt-get update
	sudo apt-get install -y libncurses5-dev libncursesw5-dev

.PHONY: clean install-deps