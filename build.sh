#!/bin/sh
CC=cc
PRG=lumasum
set -e
set -x
$CC -Wall -static -O3 -pedantic -o $PRG $PRG.c -lm
