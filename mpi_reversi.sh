#!/bin/bash
make clean
make
rm -f *txt
mpirun -n $1 ./reversi
