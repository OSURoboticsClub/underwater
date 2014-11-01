BIN := test

all: $(BIN)

### Directories ###

OBJ_DIR = .obj
OBJ_DEP_DIR = .odep

$(OBJ_DIR) $(OBJ_DEP_DIR):
	@mkdir $@

### Lists of files ###

CSRC := $(wildcard *.c)

OBJ := $(CSRC:%.c=$(OBJ_DIR)/%.o)

OBJ_DEP := $(OBJ:$(OBJ_DIR)/%.o=$(OBJ_DEP_DIR)/%.o.d)

ifneq ($(MAKECMDGOALS),clean)
-include $(OBJ_DEP)
endif

### Commands ###

RM = rm -f
RMDIR = rm -rf
CC = gcc

### Compiler setup ###

CSTD = -std=c99

CFLAGS = -Wall
CFLAGS += -Werror-implicit-function-declaration
CFLAGS += $(CSTD)

### Targets ###

clang: CC = clang
clang: LDFLAGS += -lstdc++
clang: $(BIN)

debug: CFLAGS += -g
debug: $(BIN)

$(BIN): $(OBJ)
	@echo
	@echo "=== Linking ==="
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@
	@echo
	@echo "################"
	@echo "##  Success!  ##"
	@echo "################"
	@echo

$(OBJ_DEP_DIR)/%.o.d: %.c | $(OBJ_DEP_DIR)
	@$(CC) -MM -MP -MF $@ -MT $(OBJ_DIR)/$*.o $(CFLAGS) $^

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR) $(OBJ_DEP_DIR)/%.o.d
	@echo
	@echo "=== Compiling $< ==="
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo
	@echo "=== Cleaning up ==="
	@echo
	$(RM) $(BIN)
	$(RMDIR) $(OBJ_DIR)
	$(RMDIR) $(OBJ_DEP_DIR)


### Target configuration ###

.DEFAULT_GOAL := all

.PHONY: all clang debug clean
