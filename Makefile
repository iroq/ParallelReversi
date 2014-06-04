CC=mpicc

all: reversi
reversi: reversi.o utils.o
	$(CC) -Wall -o reversi reversi.o utils.o -lncurses
utils.o: utils.c
	$(CC) -c -Wall -o utils.o utils.c
reversi.o: reversi.c
	$(CC) -c -Wall -o reversi.o reversi.c

.PHONY: all clean

clean:
	rm -f *o reversi
