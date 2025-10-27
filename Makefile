# Name: Gilad Bitton
# RedID: 130621085
# Makefile for Paging Assignment 3
CC = g++
CFLAGS = -std=c++17
OBJS = main.o pageTable.o log_helpers.o
TARGET = pagingwithpr

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.cpp pageTable.h log_helpers.h
	$(CC) $(CFLAGS) -c main.cpp

pageTable.o: pageTable.cpp pageTable.h level.h
	$(CXX) $(CXXFLAGS) -c pageTable.cpp

log_helpers.o: log_helpers.cpp log_helpers.h
	$(CXX) $(CXXFLAGS) -c log_helpers.cpp

clean:
	rm -f $(OBJS) $(TARGET)