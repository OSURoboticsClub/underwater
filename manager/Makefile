CFLAGS := -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS += -Wno-unused-function -Wno-missing-field-initializers -pedantic -g
HPATH := -I../manager

CC = gcc $(CFLAGS) $(HPATH)


../manager/manager.o: ../manager/manager.c ../manager/common.h ../manager/error.h
	$(CC) -c -o $@ $<

../manager/libworker.so: ../manager/worker.c ../manager/worker.h
	$(CC) -shared -fpic -o $@ $< -lpthread
