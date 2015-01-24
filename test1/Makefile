include ../manager/Makefile

.DEFAULT_GOAL := all
.PHONY: clean


all: main test

main: main.c ../manager/manager.o
	$(CC) -o $@ $^ -lpthread -lrt

test: test.c ../manager/libworker.so
	$(CC) -o $@ $< -lpthread -l:../manager/libworker.so

clean:
	rm -f main test
