# Name: Gilad Bitton
# RedID: 130621085
# Makefile for Paging Assignment 3
CC = g++
CFLAGS = -std=c++17
OBJS = main.o sharedData.o scheduler.o process.o log.o
TARGET = schedule

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.cpp sharedData.h scheduler.h process.h log.h
	$(CC) $(CFLAGS) -c main.cpp

process.o: process.cpp process.h
	$(CC) $(CFLAGS) -c process.cpp

sharedData.o: sharedData.cpp sharedData.h process.h
	$(CC) $(CFLAGS) -c sharedData.cpp

scheduler.o: scheduler.cpp scheduler.h sharedData.h process.h
	$(CC) $(CFLAGS) -c scheduler.cpp

log.o: log.cpp log.h
	$(CC) $(CFLAGS) -c log.cpp

clean:
	rm -f $(OBJS) $(TARGET)