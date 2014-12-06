#!/usr/bin/env bash

set -o errexit

gcc -std=c99 -Wall -Wextra -pedantic serial.c -o serial
