CC = gcc
CFLAGS = -Wall -Wextra -std=c11

TARGET = compiler
SRC = compiler.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: clean
