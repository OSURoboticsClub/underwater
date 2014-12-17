#!/usr/bin/env bash

set -o errexit

CFLAGS='-std=c99 -Wall -Wextra -Werror -Wno-unused-parameter'
CFLAGS+=' -Wno-missing-field-initializers -pedantic -g'

gcc $CFLAGS manager.c -lrt -lpthread -o manager
gcc $CFLAGS test.c worker.c -lpthread -o test
gcc $CFLAGS -shared -fpic worker.c -lpthread -o libworker.so
