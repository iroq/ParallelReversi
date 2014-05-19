all: reversi
reversi: reversi.c
	gcc -Wall -o reversi reversi.c
clean:
	rm *o reversi
