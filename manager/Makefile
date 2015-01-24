CFLAGS := -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS += -Wno-unused-function -Wno-missing-field-initializers -pedantic -g
HPATH := -I../manager

CC = gcc $(CFLAGS) $(HPATH)


../manager/manager.o: ../manager/manager.c
	$(CC) -c -o $@ $^

../manager/libworker.so: ../manager/worker.c
	$(CC) -shared -fpic -o $@ $^ -lpthread
