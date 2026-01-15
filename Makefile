CXX = g++
CXXFLAGS = -Wall -Wextra -Iinclude -std=c++11
LIBS = -lncurses

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj

SRCS = $(SRC_DIR)/main.cpp \
       $(SRC_DIR)/ProcessAnalyzer.cpp \
       $(SRC_DIR)/SystemUtils.cpp \
       $(SRC_DIR)/FilterEngine.cpp \
       $(SRC_DIR)/DisplayEngine.cpp \
       $(SRC_DIR)/ProcessSorter.cpp \
       $(SRC_DIR)/ProcessLogger.cpp

OBJS = $(OBJ_DIR)/main.o \
       $(OBJ_DIR)/ProcessAnalyzer.o \
       $(OBJ_DIR)/SystemUtils.o \
       $(OBJ_DIR)/FilterEngine.o \
       $(OBJ_DIR)/DisplayEngine.o \
       $(OBJ_DIR)/ProcessSorter.o \
       $(OBJ_DIR)/ProcessLogger.o

TARGET = pa

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean
