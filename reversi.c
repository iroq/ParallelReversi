#include <ncurses.h>
#define BOARD_SIZE 8
#define KEY_ENTER_DEF 10
#define WHITE 1
#define BLACK 2
#define NONE 3

int row,col;

void draw_board(char board[BOARD_SIZE][BOARD_SIZE])
{
	int i,j;
	clear();
	start_color();
	use_default_colors();
	init_pair(1, COLOR_BLACK, COLOR_WHITE);
	init_pair(2, COLOR_WHITE, COLOR_BLACK);
	init_pair(3, COLOR_BLUE, COLOR_BLUE);
	wborder(stdscr, '|', '|', '-', '-', '+', '+', '+', '+');
	for(i=0;i<8;i++)
	{
		for(j=0;j<16; j+=2)
		{
			if(board[i][j] == 'O')
			{
				standend();
				attron( COLOR_PAIR(1) );
			}
			else if(board[i][j] == 'X')
			{
				standend();
				attron( COLOR_PAIR(2) );
			}
			else
			{
				standend();
				attron( COLOR_PAIR(3) );
			}
			mvaddch( i+row/2-4, j+col/2-8, board[i][j] );
		}				
	}
				standend();
}

void start_new_game(char board[BOARD_SIZE][BOARD_SIZE])
{
	int usrInput, currPlayer =0, win = 0;	
	char players[] = {'O', 'X'}, c;
	MEVENT event;
	draw_board(board);
	mousemask(BUTTON1_CLICKED , NULL); 
	noecho();
	while(!win)
	{
		usrInput = getch();
		if(usrInput==KEY_MOUSE && getmouse(&event) == OK)
		{
		c = mvinch(event.y,event.x)& A_CHARTEXT;
			if(c == '-')
			{
				standend();
				attron( COLOR_PAIR(currPlayer + 1) );
				mvaddch( event.y, event.x, players[currPlayer] );
				attroff( COLOR_PAIR(currPlayer + 1) );
				currPlayer = (currPlayer + 1)%2;
			}			
		}
	}	
	return;
}
int main()
{
	char board[BOARD_SIZE][BOARD_SIZE] = 
	{{'O', 'O','O', 'O','O', 'O','O', 'O'},
	{'O', 'O','-', 'O','O', 'O','O', 'O'},
	{'O', 'O','O', 'O','O', 'O','O', 'O'},
	{'O', '-','O', 'O','O', 'O','O', 'O'},
	{'O', 'O','O', 'O','O', 'O','O', 'O'},
	{'O', 'O','O', 'O','X', 'O','O', 'O'},
	{'O', 'O','O', 'O','O', 'O','O', 'O'},
	{'O', 'O','O', 'O','O', 'O','O', 'O'}};
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
    
    move( 9, 0 );
    printw( "Koniec programu, przycisnij przycisk..." );
    getch();
    endwin();
    
}
