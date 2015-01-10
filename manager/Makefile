CFLAGS := -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS += -Wno-unused-function -Wno-missing-field-initializers -pedantic -g

all: manager test libworker.so

manager: main.c manager.c
	gcc $(CFLAGS) $^ -lrt -lpthread -o $@

test: test.c worker.c
	gcc $(CFLAGS) $^ -lpthread -o $@

libworker.so:
	gcc $(CFLAGS) -shared -fpic worker.c -lpthread -o $@

clean:
	rm -f manager test libworker.so

.DEFAULT_GOAL := all
.PHONY: clean
