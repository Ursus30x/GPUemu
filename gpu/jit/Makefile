CC  = gcc
CXX = g++

LLVM_CPPFLAGS := $(shell llvm-config --cppflags)
LLVM_CFLAGS   := $(shell llvm-config --cflags)
LLVM_LDFLAGS  := $(shell llvm-config --ldflags --libs core mcjit native executionengine --system-libs)

CFLAGS  = -Wall -Wextra -std=c11 -g $(LLVM_CPPFLAGS)
TARGET = test_jit

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LLVM_LDFLAGS)
	rm *.o
	mv $@ test/

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@