#!/usr/bin/env bash

set -o errexit

gcc -std=c99 -Wall -Wextra -Wno-unused-parameter -pedantic serial.c -lrt -o serial
