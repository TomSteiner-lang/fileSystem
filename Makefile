CC = gcc

CFLAGS = -Wall -Wextra -Werror -g -Iinclude

SRC = $(wildcard src/*.c)

OBJ = $(SRC:src/%.c=build/%.o)


TEST_SRC = tests/test_disk.c
TEST_EXE = build/test_disk

TARGET = build/filesystem

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

$(TEST_EXE): $(OBJ) $(TEST_SRC)
	mkdir -p build
	$(CC) $(CFLAGS) $(OBJ) $(TEST_SRC) -o $(TEST_EXE)

build/%.o: src/%.c
	mkdir -p build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(OBJ:.o=.d)

clean:
	rm -rf build

test_disk: $(TEST_EXE)
	./$(TEST_EXE)

.PHONY: all clean test_disk