#!/usr/bin/env bash

set -o errexit

CFLAGS='-std=c99 -Wall -Wextra -Werror -Wno-unused-parameter'
CFLAGS+=' -Wno-missing-field-initializers -pedantic -g'

gcc $CFLAGS serial.c -lrt -lpthread -o serial
gcc $CFLAGS test.c -o test
