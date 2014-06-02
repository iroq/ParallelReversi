all: reversi
reversi: reversi.o utils.o
	mpicc -Wall -o reversi reversi.o utils.o -lncurses
utils.o: utils.c
	mpicc -c -Wall -o utils.o utils.c
reversi.o: reversi.c
	mpicc -c -Wall -o reversi.o reversi.c

.PHONY: all clean

clean:
	rm -f *o reversi
