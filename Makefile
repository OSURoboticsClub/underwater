test: .obj/test.o

print-info: .obj/print-info.o

BIN := test print-info

### Lists of files ###

CSRC := $(wildcard *.c)

OBJ := $(CSRC:%.c=.obj/%.o)

OBJ_DEP := $(OBJ:.obj/%.o=.odep/%.o.d)

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

all: $(BIN)

$(BIN): .obj/grab.o .obj/utils.o
	@echo
	@echo "=== Linking ==="
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@
	@echo
	@echo "################"
	@echo "##  Success!  ##"
	@echo "################"
	@echo

.odep/%.o.d: %.c | .odep
	@$(CC) -MM -MP -MF $@ -MT .obj/$*.o $(CFLAGS) $^

.obj/%.o: %.c | .obj .odep/%.o.d
	@echo
	@echo "=== Compiling $< ==="
	$(CC) $(CFLAGS) -c $< -o $@

.obj .odep:
	@mkdir $@

clean:
	@echo
	@echo "=== Cleaning up ==="
	@echo
	$(RM) $(BIN)
	$(RMDIR) .obj
	$(RMDIR) .odep

### Target configuration ###

.DEFAULT_GOAL := all

.PHONY: all clang debug clean
