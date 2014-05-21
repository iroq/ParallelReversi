#include <ncurses.h>
#include <stdlib.h>
#include "utils.h"
#define BOARD_SIZE 8
#define KEY_ENTER_DEF 10
#define WHITE 1
#define BLACK 2
#define NONE 3
#define POSSIBLE 4
#define board2screen_row(x) x+row/2-BOARD_SIZE/2
#define board2screen_col(x) x*2+col/2-BOARD_SIZE
#define screen2board_row(x) x - (row/2-BOARD_SIZE/2)
#define screen2board_col(x) (x - (col/2-BOARD_SIZE))/2

int row,col;
  
int in_bounds(int x, int y)
{
    return (x >= 0 && y >= 0 && x < BOARD_SIZE && y < BOARD_SIZE) ? 1 : 0;
}

char opponent(char player)
{
    switch (player)
    {
    case 'X':
        return 'O';
    case 'O':
        return 'X';
    default:
        return '-';
    }
}
 
void draw_board(char board[BOARD_SIZE][BOARD_SIZE])
{
	int i,j;
	clear();
	start_color();
	use_default_colors();
	init_pair(WHITE, COLOR_BLACK, COLOR_WHITE);
	init_pair(BLACK, COLOR_WHITE, COLOR_BLACK);
	init_pair(NONE, COLOR_BLUE, COLOR_BLUE);
	init_pair(POSSIBLE, COLOR_BLACK, COLOR_RED);
	wborder(stdscr, '|', '|', '-', '-', '+', '+', '+', '+');
	for(i=0;i<BOARD_SIZE;i++)
	{
		for(j=0;j<BOARD_SIZE; j++)
		{
			if(board[i][j] == 'O')
			{
				standend();
				attron( COLOR_PAIR(WHITE) );
			}
			else if(board[i][j] == 'X')
			{
				standend();
				attron( COLOR_PAIR(BLACK) );
			}
			else
			{
				standend();
				attron( COLOR_PAIR(NONE) );
			}
			mvaddch( board2screen_row(i), board2screen_col(j), board[i][j] );
		}				
	}
	standend();
}
/* This function assumes that the move is legal. */
void make_move(int x, int y, char player, char board[BOARD_SIZE][BOARD_SIZE])
{
	int Ox=x, Oy=y;
	int dx,dy;

	board[x][y]=player;
	for(dx=-1; dx<=1; dx++)
		for(dy=-1; dy<=1; dy++)
		{
			x=Ox;
			y=Oy;
			x+=dx;
			y+=dy;
			while(betw(x,0,BOARD_SIZE-1) && betw(y,0,BOARD_SIZE-1)
				&& board[x][y]==opponent(player))
			{
				board[x][y]=player;
				x+=dx;
				y+=dy;
			}
			if(betw(x,0,BOARD_SIZE-1)==FALSE || betw(y,0,BOARD_SIZE-1)==FALSE
				|| board[x][y]!=player)
			{
				/* Backtrack */
				x-=dx;
				y-=dy;
				while(x!=Ox||y!=Oy)
				{
					board[x][y]=opponent(player);
					x-=dx;
					y-=dy;
				}
			}
			
		}
}
	
int is_legal_move(int x, int y, char board[BOARD_SIZE][BOARD_SIZE], char currentPlayer)
{
    int i, d, xx, yy;
    int sawOther, sawSelf;
    int dx[] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    int dy[] = { -1, -1, -1, 0, 0, 1, 1, 1 };
      
    if (in_bounds(x, y) && board[x][y] == '-') // in bounds, empty
    {
		for (i = 0; i < BOARD_SIZE; i++)
		{
			sawOther = 0;
			sawSelf = 0;
			xx = x;
			yy = y;
			for (d = 0; d < BOARD_SIZE; d++)
			{
				xx += dx[i];
				yy += dy[i];
				if (!in_bounds(xx, yy)) break;
				  
				if (board[xx][yy] == currentPlayer)
				{
				    sawSelf = 1;
				    break;
				}
				else if (board[xx][yy] == opponent(currentPlayer)) sawOther = 1;
				else break;
			}
			  
			if (sawOther && sawSelf)
				return 1;  
		}    
		return 0;
    }
    return 0;
}

int find_possible_moves(int moves[BOARD_SIZE * BOARD_SIZE][2], char board[BOARD_SIZE][BOARD_SIZE], char currentPlayer)
{
    int i, j, counter = 0;
    for (i = 0; i < BOARD_SIZE; i++)
    {
        for (j = 0; j < BOARD_SIZE; j++)
        {
            if (is_legal_move(i, j, board, currentPlayer))
            {
				//mvprintw(counter, 3, "[%d, %d]\n", i, j);
                moves[counter][0] = board2screen_row(i);
                moves[counter][1] = board2screen_col(j);
                counter++;
            }
        }
    }
	return counter;
}



void start_new_game(char board[BOARD_SIZE][BOARD_SIZE])
{
	int i, usrInput, currPlayer =0, turnCounter = 4, possibleMoves[BOARD_SIZE * BOARD_SIZE][2], moves, x, y;
	short clickedColor;	
	char players[] = {'O', 'X'};
	MEVENT event;
	draw_board(board);
	mousemask(BUTTON1_CLICKED , NULL); 
	noecho();

	moves = find_possible_moves(possibleMoves, board, players[currPlayer]);
	for(i = 0; i < moves; i++)
	{
		attron( COLOR_PAIR(POSSIBLE) );
		mvaddch( possibleMoves[i][0], possibleMoves[i][1], '-');
		attroff( COLOR_PAIR(POSSIBLE) );
	}
	mvprintw(1, 1, "PLAYER: %c", players[currPlayer]);

	while(turnCounter!= BOARD_SIZE*BOARD_SIZE)
	{
		usrInput = getch();
		if(usrInput==KEY_MOUSE && getmouse(&event) == OK)
		{
			clickedColor = (mvinch(event.y, event.x) & A_COLOR) >> 8;
			x = screen2board_row(event.y);
			y = screen2board_col(event.x);
			if(clickedColor == POSSIBLE)
			{
				standend();
				attron( COLOR_PAIR(currPlayer + 1) );
				mvaddch( event.y, event.x, players[currPlayer] );
				attroff( COLOR_PAIR(currPlayer + 1) );
				make_move(x, y, players[currPlayer], board);
				draw_board(board);				

				currPlayer = (currPlayer + 1)%2;	
				moves = find_possible_moves(possibleMoves, board, players[currPlayer]);
				for(i = 0; i < moves; i++)
				{
					attron( COLOR_PAIR(POSSIBLE) );
					mvaddch( possibleMoves[i][0], possibleMoves[i][1], '-');
					attroff( COLOR_PAIR(POSSIBLE) );
				}
				mvprintw( 1, 1, "%c", players[currPlayer]);
				turnCounter++;			
			}			
		}
	}	
	return;
}

void init_board(char board[BOARD_SIZE][BOARD_SIZE])
{
	int i,j;
	int half = BOARD_SIZE/2;

	for(i = 0; i < 8; i++)
	{
		for(j = 0; j < 8; j++)
		{
			board[i][j] = '-';				
		}
	}

	board[half - 1][half - 1] = 'O';
	board[half - 1][half] = 'X';
	board[half][half] = 'O';
	board[half][half - 1] = 'X';
}

void display_menu(char board[BOARD_SIZE][BOARD_SIZE])
{
	initscr();
    curs_set(0);
    keypad(stdscr, TRUE);
    getmaxyx(stdscr,row,col);
    const char txt1[] = "New game";
    const char txt2[] = "Exit";
    int currItem = 1;
    int usrInput = KEY_UP;
    const short int minItemNr = 1;
    const short int maxItemNr = 2;
    do
    {   
	    switch( currItem )
        {
	        case 1:
	            mvprintw( row/2, col/2 - 2, txt2 );
	            attron( A_REVERSE );
	            mvprintw( row/2-1, col/2 - 4, txt1 );
	            break;
	            
	        case 2:
	            mvprintw( row/2-1, col/2 - 4, txt1 );
	            attron( A_REVERSE );
	            mvprintw( row/2, col/2 - 2, txt2 );
	            break;            
        }   
        attroff( A_REVERSE );
                
        usrInput = getch();
        if( usrInput == KEY_UP && currItem > minItemNr )
        {
            currItem--;
        }
        else if( usrInput == KEY_DOWN && currItem < maxItemNr )
        {
            currItem++;
        }  
        if( usrInput == KEY_ENTER_DEF)
        {
            switch( currItem )
            {
	            case 1:
	            start_new_game(board);		
	            break;
	        }            
        }
        standend();
        clear();
    } while( currItem != 2 || usrInput != 10 );
}

int main()
{	
	char board[BOARD_SIZE][BOARD_SIZE];	
	init_board(board);
    display_menu(board);    
    printw( "Thanks for playing!" );
    getch();
    endwin();   
    return 0; 
}
