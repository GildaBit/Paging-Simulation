# Name: Gilad Bitton
# RedID: 130621085
# Makefile for Paging Assignment 3
CC = g++
CFLAGS = -std=c++17
OBJS = main.o pageTable.o log_helpers.o level.o vaddr_tracereader.o nfu.o
TARGET = pagingwithpr

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.cpp pageTable.h log_helpers.h vaddr_tracereader.h nfu.h
	$(CC) $(CFLAGS) -c main.cpp

pageTable.o: pageTable.cpp pageTable.h level.h
	$(CXX) $(CXXFLAGS) -c pageTable.cpp

log_helpers.o: log_helpers.cpp log_helpers.h
	$(CXX) $(CXXFLAGS) -c log_helpers.cpp

level.o: level.cpp level.h map.h
	$(CXX) $(CXXFLAGS) -c level.cpp

vaddr_tracereader.o: vaddr_tracereader.cpp vaddr_tracereader.h 
		$(CXX) $(CXXFLAGS) -c vaddr_tracereader.cpp

nfu.o: nfu.cpp nfu.h 
	$(CXX) $(CXXFLAGS) -c nfu.cpp

clean:
	rm -f $(OBJS) $(TARGET)
