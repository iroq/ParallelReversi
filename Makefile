all: reversi
reversi: reversi.c
	gcc -Wall -o reversi reversi.c -lncurses
clean:
	rm *o reversi
