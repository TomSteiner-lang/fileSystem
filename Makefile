CC = gcc

CFLAGS = -Wall -Wextra -Werror -g -Iinclude

SRC = $(wildcard src/*.c)

OBJ = $(SRC:src/%.c=build/%.o)

TARGET = build/filesystem

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

build/%.o: src/%.c
	mkdir -p build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(OBJ:.o=.d)

clean:
	rm -rf build

.PHONY: all clean