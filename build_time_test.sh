#!/usr/bin/env sh
gcc -Wall -fPIC -O2 -fcommon -std=gnu99 time_test.c -o time_test
gcc -Wall -fPIC -O2 -fcommon -std=gnu99 -D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64 time_test.c -o time_test64
