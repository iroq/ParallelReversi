#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
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

void make_move(char board[BOARD_SIZE][BOARD_SIZE], int x, int y, char player);
int find_possible_moves(char board[BOARD_SIZE][BOARD_SIZE], int moves[BOARD_SIZE * BOARD_SIZE][2], char currentPlayer);

//for ncurses
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

void copy_board(char src[BOARD_SIZE][BOARD_SIZE], char dest[BOARD_SIZE][BOARD_SIZE])
{
	int i,j;
	for(i=0; i<BOARD_SIZE; i++)
		for(j=0; j<BOARD_SIZE; j++)
			dest[i][j]=src[i][j];
}

int heur_sc(char board[BOARD_SIZE][BOARD_SIZE], char player)
{
	int i,j;
	int count=0;
	for(i=0;i<BOARD_SIZE;i++)
		for(j=0;j<BOARD_SIZE;j++)
			if(board[i][j]==player)
				count++;
	return count;
}


int alpha_beta(char board[BOARD_SIZE][BOARD_SIZE], int pos_moves[BOARD_SIZE*BOARD_SIZE][2], int moves_c, char player)
{
	int i, val, x, y,max=0, ret=0;
	char temp[BOARD_SIZE][BOARD_SIZE];
	for(i=0; i<moves_c; i++)
	{
		x=pos_moves[i][0];
		y=pos_moves[i][1];
		copy_board(board, temp);
		make_move(temp, x,y,player);
		val=heur_sc(temp, player);
		if(val>max)
		{
			max=val;
			ret=i;
		}
	}
	return ret;
}

int alpha_beta_r(char board[BOARD_SIZE][BOARD_SIZE], int depth, int a, int b, char player, bool is_opp)
{
	if(depth==0)
		return heur_sc(board, player);
	int pos_moves[BOARD_SIZE*BOARD_SIZE][2];
	int moves_c, i;
	moves_c=find_possible_moves(board, pos_moves, player);
	if(moves_c==0)
		return heur_sc(board,player);
	char temp[BOARD_SIZE][BOARD_SIZE];
	if(is_opp)
	{
		//Opponent move
		for(i=0;i<moves_c;i++)
		{
			copy_board(board, temp);
			make_move(temp, pos_moves[i][0], pos_moves[i][1], player);
			b=min(b, alpha_beta_r(temp, depth-1, a, b, opponent(player), !is_opp));
			if(a>=b)
				break;
		}
		return b;
	}else
	{
		//Player move
		for(i=0;i<moves_c;i++)
		{
			copy_board(board, temp);
			make_move(temp, pos_moves[i][0], pos_moves[i][1], player);
			a=max(a, alpha_beta_r(temp, depth-1, a, b, opponent(player), !is_opp));
			if(a>=b)
				break;
		}
		return a;
	}

}

void count_stones(int *xcount, int *ocount, char board[BOARD_SIZE][BOARD_SIZE])
{
	int i,j;
	*xcount=0;
	*ocount=0;
	for(i=0;i<BOARD_SIZE;i++)
		for(j=0;j<BOARD_SIZE;j++)
		{
			if(board[i][j]=='X')
				(*xcount)++;
			else if(board[i][j]=='O')
				(*ocount)++;
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
void make_move(char board[BOARD_SIZE][BOARD_SIZE], int x, int y, char player)
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
	
int is_legal_move(char board[BOARD_SIZE][BOARD_SIZE], int x, int y, char currentPlayer)
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

int find_possible_moves(char board[BOARD_SIZE][BOARD_SIZE], int moves[BOARD_SIZE * BOARD_SIZE][2], char currentPlayer)
{
    int i, j, counter = 0;
    for (i = 0; i < BOARD_SIZE; i++)
    {
        for (j = 0; j < BOARD_SIZE; j++)
        {
            if (is_legal_move(board, i, j, currentPlayer))
            {
				//mvprintw(counter, 3, "[%d, %d]\n", i, j);
                moves[counter][0] = i;
                moves[counter][1] = j;
                counter++;
            }
        }
    }
	return counter;
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

void start_new_game(char board[BOARD_SIZE][BOARD_SIZE])
{
	int i, usrInput, currPlayer =0, turnCounter = 4, possibleMoves[BOARD_SIZE * BOARD_SIZE][2], moves, x, y;
	int xcount, ocount, move_was_possible;
	short clickedColor;	
	char players[] = {'O', 'X'};
	MEVENT event;
	init_board(board);
	mousemask(BUTTON1_CLICKED , NULL); 
	noecho();
	
	moves = find_possible_moves(board, possibleMoves, players[currPlayer]);
	draw_board(board);
	for(i = 0; i < moves; i++)
	{
		attron( COLOR_PAIR(POSSIBLE) );
		mvaddch( board2screen_row(possibleMoves[i][0]), board2screen_col(possibleMoves[i][1]), '-');
		attroff( COLOR_PAIR(POSSIBLE) );
	}
	mvprintw( 1, 1, "PLAYER: %c", players[currPlayer]);

	move_was_possible=TRUE;
	while(turnCounter<BOARD_SIZE*BOARD_SIZE)
	{
		bool move_made=false;
		if(turnCounter%2==0)
		{
			/* Player move */
			usrInput = getch();
			if(usrInput==KEY_MOUSE && getmouse(&event) == OK)
			{
				clickedColor = (mvinch(event.y, event.x) & A_COLOR) >> 8;
				if(clickedColor == POSSIBLE)
				{
					move_made=true;
					x = screen2board_row(event.y);
					y = screen2board_col(event.x);
				}		
			}	
		}else
		{
			/* Omega move */
			sleep(2);
			int index=alpha_beta(board, possibleMoves, moves, players[currPlayer]);
			x=possibleMoves[index][0];
			y=possibleMoves[index][1];
			move_made=true;
		}
		if(move_made)
		{
			standend();
			attron( COLOR_PAIR(currPlayer + 1) );
			mvaddch( event.y, event.x, players[currPlayer] );
			attroff( COLOR_PAIR(currPlayer + 1) );
			make_move(board, x, y, players[currPlayer]);
			count_stones(&xcount, &ocount, board);
			draw_board(board);
			mvprintw(2,1, "X SCORE: %d", xcount);
			mvprintw(3,1, "O SCORE: %d", ocount);			

			currPlayer = (currPlayer + 1)%2;	
			moves = find_possible_moves(board, possibleMoves, players[currPlayer]);
			if(moves==0)
			{
				if(!move_was_possible)
					break;
				else
					move_was_possible=FALSE;
			}
				
			for(i = 0; i < moves; i++)
			{
				attron( COLOR_PAIR(POSSIBLE) );
				mvaddch( board2screen_row(possibleMoves[i][0]), board2screen_col(possibleMoves[i][1]), '-');
				attroff( COLOR_PAIR(POSSIBLE) );
			}
			mvprintw( 1, 1, "PLAYER: %c", players[currPlayer]);
			refresh();
			turnCounter++;			
		}
	}
	clear();
	if(xcount==ocount)
		mvprintw(row/2, (col/2)-2, "TIE!");
	else
		mvprintw(row/2, (col/2)-5, "PLAYER: %c WON!", (xcount<ocount)?'O':'X');
	getch();
	return;
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
