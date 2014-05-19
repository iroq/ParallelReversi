#include <ncurses.h>
#define BOARD_SIZE 8
#define KEY_ENTER_DEF 10
#define WHITE 1
#define BLACK 2
#define NONE 3
#define board2screen_row(x) x+row/2-BOARD_SIZE/2
#define board2screen_col(x) x+col/2-BOARD_SIZE
#define screen2board_row(x) x - (row/2-BOARD_SIZE/2)
#define screen2board_col(x) (x - (col/2-BOARD_SIZE))/2

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
	for(i=0;i<BOARD_SIZE;i++)
	{
		for(j=0;j<BOARD_SIZE*2; j+=2)
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
			mvaddch( board2screen_row(i), board2screen_col(j), board[i][j] );
		}				
	}
	standend();
}

void start_new_game(char board[BOARD_SIZE][BOARD_SIZE])
{
	int usrInput, currPlayer =0, turnCounter = 0;	
	char players[] = {'O', 'X'}, clickedChar;
	MEVENT event;
	draw_board(board);
	mousemask(BUTTON1_CLICKED , NULL); 
	noecho();
	while(turnCounter!= BOARD_SIZE*BOARD_SIZE)
	{
		usrInput = getch();
		if(usrInput==KEY_MOUSE && getmouse(&event) == OK)
		{
			clickedChar = mvinch(event.y,event.x)& A_CHARTEXT;
			if(clickedChar == '-')
			{
				standend();
				attron( COLOR_PAIR(currPlayer + 1) );
				mvaddch( event.y, event.x, players[currPlayer] );
				attroff( COLOR_PAIR(currPlayer + 1) );
				board[screen2board_row(event.y)][screen2board_col(event.x)] = players[currPlayer];
				mvprintw( 0, 0, "%d", screen2board_row(event.y));//debug
				mvprintw( 1, 0, "%d", screen2board_col(event.x));
				currPlayer = (currPlayer + 1)%2;	
				turnCounter++;			
			}			
		}
	}	
	return;
}

void init_board(char board[BOARD_SIZE][BOARD_SIZE])
{
	int i,j;
	for(i = 0; i < 8; i++)
	{
		for(j = 0; j < 8; j++)
		{
			board[i][j] = '-';				
		}
	}
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
