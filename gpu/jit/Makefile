CC  = gcc
CXX = g++

# Use --cppflags for the C compiler to get includes/defines
LLVM_CPPFLAGS := $(shell llvm-config --cppflags)
# Use --cflags only if you really need the machine-specific optimization flags
LLVM_CFLAGS   := $(shell llvm-config --cflags)
# Added --system-libs to prevent boundary errors
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