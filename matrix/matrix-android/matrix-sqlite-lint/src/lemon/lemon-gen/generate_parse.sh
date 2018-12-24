#!/bin/bash
rm -rf parse.h parse.h.temp parse.c
gcc -o lemon lemon.c
gcc -o mkkeywordhash mkkeywordhash.c
./lemon parse.y
./mkkeywordhash > keywordhash.h

mv parse.h parse.h.temp
awk -f addopcodes.awk parse.h.temp > parse.h
