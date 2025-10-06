CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
LDFLAGS = -lncurses -lpthread
TARGET = process_analyzer
SRC = process_analyzer.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) process_log.csv

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
