#!/bin/sh

killall -9 test_prg
rm /dev/shm/sem*
rm /dev/shm/Game*
rm /dev/mqueue/*
tput init
stty sane
killall -9 ./test_prg
make clean
make

