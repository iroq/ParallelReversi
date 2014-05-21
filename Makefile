all: reversi
reversi: reversi.o utils.o
	gcc -Wall -o reversi reversi.o utils.o -lncurses
utils.o: utils.c
	gcc -c -Wall -o utils.o utils.c
reversi.o: reversi.c
	gcc -c -Wall -o reversi.o reversi.c

.PHONY: all clean

clean:
	rm -f *o reversi
