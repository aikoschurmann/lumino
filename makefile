# Directories
OUT_DIR = out
SRC_DIR = src
TEST_DIR = test
OBJ_DIR = obj

# Compiler
CC = gcc

# Project name
NAME = Lumino

# Source files
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
TEST_FILES = $(wildcard $(TEST_DIR)/*.c)

# Object files
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES)) \
            $(patsubst $(TEST_DIR)/%.c,$(OBJ_DIR)/%.o,$(TEST_FILES))

# Dependency files
DEP_FILES = $(OBJ_FILES:.o=.d)

# Flags
CFLAGS = $(shell sdl2-config --cflags) -Iinclude -MMD -MP -g -O3 -mavx2 -mfma

LDFLAGS = $(shell sdl2-config --libs) -lSDL2_image -lm

# Targets
all: $(OUT_DIR)/$(NAME)

$(OUT_DIR)/$(NAME): $(OBJ_FILES)
	$(CC) $(OBJ_FILES) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Include dependencies
-include $(DEP_FILES)

# Clean up object files and binaries
clean:
	rm -rf $(OBJ_DIR)/* $(OUT_DIR)/*


# Run tests
run: all
	./$(OUT_DIR)/$(NAME)

.PHONY: all clean run
