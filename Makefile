CC := gcc

CFLAGS := -Wall -Wextra -Werror -g -Iinclude

# Source files
SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))

# Tests
TEST_SRC := $(wildcard tests/test_*.c)
TEST_EXE := $(patsubst tests/%.c,build/tests/%,$(TEST_SRC))

# Default target
all: $(OBJ)

# Compile library objects
build/%.o: src/%.c
	mkdir -p build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Build one test executable
build/tests/test_%: tests/test_%.c $(OBJ)
	mkdir -p build/tests
	$(CC) $(CFLAGS) $(OBJ) $< -o $@

# Run every test
test: $(TEST_EXE)
	@for test in $(TEST_EXE); do \
		echo "Running $$test"; \
		./$$test; \
	done

# Remove build directory
clean:
	rm -rf build

# Dependency files
-include $(OBJ:.o=.d)

.PHONY: all test clean