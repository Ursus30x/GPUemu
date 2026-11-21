CC = gcc
CFLAGS = -Wall -Wextra -std=c11

TARGET = ../tools/compiler
SRC = compiler.c

all: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p ../tools
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) compiler

.PHONY: all clean
