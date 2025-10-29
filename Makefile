# Name: Gilad Bitton
# RedID: 130621085
# Makefile for Paging Assignment 3 (folders-aware)

# Compiler / flags
CXX       = g++
CXXFLAGS  = -std=c++17 -Wall -Wextra -MMD -MP

# Directories
SRC_DIR   = code_files/cpp_files
OBJ_DIR   = object_files
INC_DIRS  = code_files/header_files code_files

# Apply include dirs
CXXFLAGS += $(addprefix -I,$(INC_DIRS))

# Sources / Objects / Deps / Target
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

TARGET = pagingwithpr

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Ensure object dir exists, then compile each .cpp -> .o
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Include auto-generated dependency files
-include $(DEPS)

# Convenience run target (adjust args to your needs)
run: $(TARGET)
	./$(TARGET) -n 50 -f 20 -b 10 -l vpn2pfn_pr input_files/trace.tr 6 6 8

clean:
	rm -f $(OBJ_DIR)/*.o $(OBJ_DIR)/*.d $(TARGET)