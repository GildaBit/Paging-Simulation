# Name: Gilad Bitton
# RedID: 130621085
# Makefile for Paging Assignment 3
CC = g++
CFLAGS = -std=c++17
OBJS = main.o
TARGET = pagingwithpr

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.cpp 
	$(CC) $(CFLAGS) -c main.cpp

clean:
	rm -f $(OBJS) $(TARGET)