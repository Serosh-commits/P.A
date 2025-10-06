CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
LDFLAGS = -lncurses
TARGET = process_analyzer
SOURCE = process_analyzer.cpp

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)

clean:
	rm -f $(TARGET) process_log.csv

install:
	sudo cp $(TARGET) /usr/local/bin/

.PHONY: clean install