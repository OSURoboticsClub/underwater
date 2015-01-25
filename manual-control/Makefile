include ../manager/Makefile

.DEFAULT_GOAL := all
.PHONY: clean


all: main keyboard printer

main: main.c ../manager/manager.o
	$(CC) -o $@ $^ -lpthread -lrt

keyboard: keyboard.c ../manager/libworker.so
	$(CC) -o $@ $< -lpthread -l:../manager/libworker.so

printer: printer.c ../manager/libworker.so
	$(CC) -o $@ $< -lpthread -l:../manager/libworker.so

clean:
	rm -f main manual
