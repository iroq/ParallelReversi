#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>
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
#define ABDEPTH 6
#define HEURISTIC(board, player) heur_sc(board, player)

typedef struct data_
{
	char board[BOARD_SIZE][BOARD_SIZE];
	char player;
	int depth;
	int index;
	int job_no;
} data;

typedef struct sldata_
{
	int ret_val;
	int index;
} sldata;

void make_move(char board[BOARD_SIZE][BOARD_SIZE], int x, int y, char player);
int find_possible_moves(char board[BOARD_SIZE][BOARD_SIZE], int moves[BOARD_SIZE * BOARD_SIZE][2], char currentPlayer);
int alpha_beta_r(char board[BOARD_SIZE][BOARD_SIZE], int depth, int a, int b, char player, bool is_opp);

//for ncurses
int row,col;
MPI_Datatype mpi_msg_type;
MPI_Datatype mpi_slmsg_type;
 
int MPI_job_counter=100;
int myrank;
 
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

int heur_mob(char board[BOARD_SIZE][BOARD_SIZE], char player)
{
	int i, j, k, l;
	// mobility of the player
	int mob_pl = find_possible_moves(board, NULL, player);
	// potential mobility of the player
	int pot_mob_pl;
	// potential mobility of the opponent
	int pot_mob_opp;
	int opp_neighbour, pl_neighbour;

	for (i = 0; i < BOARD_SIZE; i++)
		for (j = 0; j < BOARD_SIZE; j++)
		{
			mvprintw(5, 5, "[%d, %d]\n", i, j);
			// check if the slot is next to at least one disk belonging to the player/opponent
			if (board[i][j] == '-')
			{
				opp_neighbour = pl_neighbour = 0;
				for (k = i - 1; k <= i + 1; k++)
				{
					if (opp_neighbour && pl_neighbour)
						break;
					for (l = j - 1; l <= j + 1; l++)
					{
						if (l < 0 || l >= BOARD_SIZE || k < 0 || k >= BOARD_SIZE)
							continue;
						if (opp_neighbour && pl_neighbour)
							break;
						if (!pl_neighbour && board[k][l] == opponent(player))
							pl_neighbour = 1;
						if (!opp_neighbour && board[k][l] == player)
							opp_neighbour = 1;
					}
				}
				if (opp_neighbour)
					pot_mob_opp++;
				if (pl_neighbour)
					pot_mob_pl++;
			}
		}
	return mob_pl + pot_mob_pl - pot_mob_opp;	
}

int isCorner(int i, int j)
{
	return ((i == 0 && j == 0)
			|| (i == 0 && j == BOARD_SIZE - 1)
			|| (i == BOARD_SIZE - 1 && j == 0)
			|| (i == BOARD_SIZE - 1 && j == BOARD_SIZE - 1));
}

int heur_mob_cor(char board[BOARD_SIZE][BOARD_SIZE], char player)
{
	int corner_weight = 4;
	int result = 0;
	int i, j, k, l;
	// mobility of the player
	int mob_pl = find_possible_moves(board, NULL, player);
	// potential mobility of the player
	int pot_mob_pl = 0;
	// potential mobility of the opponent
	int pot_mob_opp = 0;
	int opp_neighbour, pl_neighbour;

	for (i = 0; i < BOARD_SIZE; i++)
		for (j = 0; j < BOARD_SIZE; j++)
		{
			mvprintw(5, 5, "[%d, %d]\n", i, j);
			// check if the slot is next to at least one disk belonging to the player/opponent
			if (board[i][j] == '-')
			{
				opp_neighbour = pl_neighbour = 0;
				for (k = i - 1; k <= i + 1; k++)
				{
					if (opp_neighbour && pl_neighbour)
						break;
					for (l = j - 1; l <= j + 1; l++)
					{
						if (l < 0 || l >= BOARD_SIZE || k < 0 || k >= BOARD_SIZE)
							continue;
						if (opp_neighbour && pl_neighbour)
							break;
						if (!pl_neighbour && board[k][l] == opponent(player))
							pl_neighbour = 1;
						if (!opp_neighbour && board[k][l] == player)
							opp_neighbour = 1;
					}
				}
				if (opp_neighbour)
					pot_mob_opp += isCorner(i, j) ? corner_weight : 1;
				if (pl_neighbour)
					pot_mob_pl += isCorner(i, j) ? corner_weight : 1;
			}
		}
	result = mob_pl + pot_mob_pl - pot_mob_opp;	

	if (board[0][BOARD_SIZE - 1] == player)
		result += corner_weight;
	if (board[0][BOARD_SIZE - 1] == opponent(player))
		result -= corner_weight;
	if (board[0][0] == player)
		result += corner_weight;
	if (board[0][0] == opponent(player))
		result -= corner_weight;
	if (board[BOARD_SIZE - 1][BOARD_SIZE - 1] == player)
		result += corner_weight;
	if (board[BOARD_SIZE - 1][BOARD_SIZE - 1] == opponent(player))
		result -= corner_weight;
	if (board[BOARD_SIZE - 1][0] == player)
		result += corner_weight;
	if (board[BOARD_SIZE - 1][0] == opponent(player))
		result -= corner_weight;
			
	return result;
}
void create_struct()
{
	const int nitems=5, nitems1 = 2;
    int blocklengths[5] = {BOARD_SIZE * BOARD_SIZE,1,1,1,1};
    int blocklengths1[2] = {1,1};
    MPI_Datatype types[5] = {MPI_CHAR, MPI_CHAR, MPI_INT, MPI_INT, MPI_INT};
    MPI_Datatype types1[2] = {MPI_INT, MPI_INT};

    MPI_Aint offsets[nitems];
    MPI_Aint offsets1[nitems1];

    offsets[0] = offsetof(data, board);
    offsets[1] = offsetof(data, player);
    offsets[2] = offsetof(data, depth);
    offsets[3] = offsetof(data, index);
    offsets[4] = offsetof(data, job_no);
    
    offsets1[0] = offsetof(sldata, ret_val);
    offsets1[1] = offsetof(sldata, index);

    MPI_Type_create_struct(nitems, blocklengths, offsets, types, &mpi_msg_type);
    MPI_Type_commit(&mpi_msg_type);
    
    MPI_Type_create_struct(nitems1, blocklengths1, offsets1, types1, &mpi_slmsg_type);
    MPI_Type_commit(&mpi_slmsg_type);
}

void print_log(char *name, const char *format, ... )
{
	va_list arglist;
	
	char buf[50];
	sprintf(buf, "%s.txt", name);
	FILE *f = fopen(buf, "a+");
	if (f == NULL)
	{
	    printf("Error opening file!\n");
	    exit(1);
	}
	fprintf(f, "[%d]: ", getpid());
	va_start(arglist, format);
	vfprintf(f, format, arglist);
	va_end(arglist);
	fclose(f);
}

int alpha_beta(char board[BOARD_SIZE][BOARD_SIZE], int pos_moves[BOARD_SIZE*BOARD_SIZE][2], int moves_c, char player)
{
	int i, x, y,max=0, ret=0;
	int rank_c;
	data msg;
	sldata slmsg;
	MPI_Status status;
	MPI_Comm_size(MPI_COMM_WORLD, &rank_c); 
	for(i=0; i<moves_c; i++)
	{
		x=pos_moves[i][0];
		y=pos_moves[i][1];
		copy_board(board, msg.board);
		make_move(msg.board, x,y,player);
		msg.player = player;
		msg.depth = ABDEPTH;
		msg.index = i;
		MPI_Send(&msg, sizeof(data), mpi_msg_type, (i % (rank_c-1))+1, 0, MPI_COMM_WORLD);
	}
	for(i=0; i<moves_c; i++)
	{
		MPI_Recv((void*)&slmsg, sizeof(sldata), mpi_slmsg_type, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		if(slmsg.ret_val > max)
		{
			max=slmsg.ret_val;
			ret=slmsg.index;
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

int alpha_beta_pvs_r(char board[BOARD_SIZE][BOARD_SIZE], int depth, int a, int b, char player)
{
	if(depth==0)
		return HEURISTIC(board, player);
	
	int pos_moves[BOARD_SIZE*BOARD_SIZE][2];
	int moves_c, i,score;
	moves_c=find_possible_moves(board, pos_moves, player);
	if(moves_c==0)
		return HEURISTIC(board, player);
	int probe=0, rank_c, j;
	MPI_Status stat;
	MPI_Request req;
	int a_recv;
	char temp[BOARD_SIZE][BOARD_SIZE];	
	MPI_Comm_size(MPI_COMM_WORLD, &rank_c); 
	for(i=0; i<moves_c; i++)
	{
		copy_board(board, temp);
		make_move(temp, pos_moves[i][0], pos_moves[i][1], player);
		score=-alpha_beta_pvs_r(temp, depth-1, -b, -a, opponent(player));
		if(score>b)
		{
			return b;
		}
		
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_job_counter, MPI_COMM_WORLD, &probe, &stat);
		if(probe)
		{
			MPI_Recv(&a_recv, 1, MPI_INT, MPI_ANY_SOURCE, MPI_job_counter, MPI_COMM_WORLD, &stat);
		}
		if(probe && same_sign(a, a_recv) && a_recv>score && a_recv > a)
		{
			a=a_recv;
		}
		else if(score>a)
		{
			a=score;
			if(depth%2==0)
			{
				for(j = 1; j < rank_c; j++)
				{
					if(myrank == j) 
						continue;
					MPI_Isend(&a,1, MPI_INT, j, MPI_job_counter, MPI_COMM_WORLD, &req);
				}
			}
		}	
	}
	return a;
}

int pv_split(char board[BOARD_SIZE][BOARD_SIZE], int depth, int a, int b, char player)
{
	if(depth==0)
		return HEURISTIC(board, player);
	int pos_moves[BOARD_SIZE*BOARD_SIZE][2];
	int moves_c, i, score;
	moves_c=find_possible_moves(board, pos_moves, player);
	if(moves_c==0)
		return HEURISTIC(board,player);
	char temp[BOARD_SIZE][BOARD_SIZE];
	int rank_c;
	data msg;
	sldata slmsg;
	MPI_Status status;
	MPI_Comm_size(MPI_COMM_WORLD, &rank_c); 
	
	copy_board(board, temp);
	make_move(temp, pos_moves[0][0], pos_moves[0][1], player);
	score=pv_split(temp, depth-1, a, b, opponent(player));
	if(score>b)
		return b;
	if(score>a)
		a=score;
	
	msg.job_no=MPI_job_counter;
	
	//Begin parallel loop
	for(i=1; i<moves_c; i++)
	{
		copy_board(board, msg.board);
		make_move(msg.board, pos_moves[i][0], pos_moves[i][1], player);
		msg.player = player;
		msg.depth = depth-1;
		msg.index = i;
		MPI_Send(&msg, 1, mpi_msg_type, (i % (rank_c-1))+1, 0, MPI_COMM_WORLD);		
	}// End parallel loop
	for(i=1; i<moves_c; i++)
	{
		MPI_Recv((void*)&slmsg, 1, mpi_slmsg_type, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		//score=alpha_beta_pvs_r(temp, depth-1, a, b, opponent(player));
		if(slmsg.ret_val>b)
			return b;
		if(slmsg.ret_val>a)
			a=slmsg.ret_val;
	}	
	return a;
}

int pv_split_master(char board[BOARD_SIZE][BOARD_SIZE], int pos_moves[BOARD_SIZE*BOARD_SIZE][2], int moves_c, char player)
{
	char temp[BOARD_SIZE][BOARD_SIZE];
	int i, score,a=INT_MIN, b=INT_MAX,index;
	int rank_c;
	data msg;
	sldata slmsg;
	MPI_Status status;
	MPI_Comm_size(MPI_COMM_WORLD, &rank_c); 
	
	copy_board(board, temp);
	make_move(temp, pos_moves[0][0], pos_moves[0][1], player);
	score=pv_split(temp, ABDEPTH-1, a, b, opponent(player));
	a=score;
	index=0;
	msg.job_no=MPI_job_counter;
	
	//Begin parallel loop
	for(i=1; i<moves_c; i++)
	{
		copy_board(board, msg.board);
		make_move(msg.board, pos_moves[i][0], pos_moves[i][1], player);
		score=alpha_beta_pvs_r(temp, ABDEPTH-1, a, b, opponent(player));
		msg.player = player;
		msg.depth = ABDEPTH-1;
		msg.index = i;
		MPI_Send(&msg, 1, mpi_msg_type, (i % (rank_c-1))+1, 0, MPI_COMM_WORLD);	
	}// End parallel loop

	for(i=1; i<moves_c; i++)
	{
		MPI_Recv((void*)&slmsg, 1, mpi_slmsg_type, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
		//score=alpha_beta_pvs_r(temp, depth-1, a, b, opponent(player));
		if(slmsg.ret_val>a)
		{
			a=slmsg.ret_val;
			index=slmsg.index;
		}
	}
	MPI_job_counter++;
	return index;
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
	
	standend();
	attron( COLOR_PAIR(NONE) );
	for(i=0;i<BOARD_SIZE;i++)
	{
		for(j=0;j<BOARD_SIZE*2 - 1; j++)
		{
			mvaddch(i + row/2-BOARD_SIZE/2, j +col/2-BOARD_SIZE, ' ');
		}
	}
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
				if (NULL != moves)
				{
		            moves[counter][0] = i;
		            moves[counter][1] = j;
				}
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
	int i, usrInput, turnCounter = 4, possibleMoves[BOARD_SIZE * BOARD_SIZE][2], moves, x, y;
	int xcount, ocount, move_was_possible;
	short clickedColor;	
	//char players[] = {'O', 'X'};
	char currPlayer = 'O';
	MEVENT event;
	init_board(board);
	mousemask(BUTTON1_CLICKED , NULL); 
	noecho();
	
	moves = find_possible_moves(board, possibleMoves, currPlayer);
	draw_board(board);
	for(i = 0; i < moves; i++)
	{
		attron( COLOR_PAIR(POSSIBLE) );
		mvaddch( board2screen_row(possibleMoves[i][0]), board2screen_col(possibleMoves[i][1]), '-');
		attroff( COLOR_PAIR(POSSIBLE) );
	}
	mvprintw( 1, 1, "PLAYER: %c", currPlayer);

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
			int index=pv_split_master(board, possibleMoves, moves, currPlayer);
			x=possibleMoves[index][0];
			y=possibleMoves[index][1];
			move_made=true;
		}
		if(move_made)
		{
			standend();
			attron( COLOR_PAIR(currPlayer == '0' ?  WHITE : BLACK) );
			mvaddch( event.y, event.x, currPlayer );
			attroff( COLOR_PAIR(currPlayer == '0' ?  WHITE : BLACK) );
			make_move(board, x, y, currPlayer);
			count_stones(&xcount, &ocount, board);
			draw_board(board);
			mvprintw(2,1, "X SCORE: %d", xcount);
			mvprintw(3,1, "O SCORE: %d", ocount);			

			currPlayer = opponent(currPlayer);	
			moves = find_possible_moves(board, possibleMoves, currPlayer);
			if(moves==0)
			{
				currPlayer = opponent(currPlayer);
				if(!move_was_possible)
					break;					
				else
					move_was_possible = false;
			}				
				
			for(i = 0; i < moves; i++)
			{
				attron( COLOR_PAIR(POSSIBLE) );
				mvaddch( board2screen_row(possibleMoves[i][0]), board2screen_col(possibleMoves[i][1]), '-');
				attroff( COLOR_PAIR(POSSIBLE) );
			}
			mvprintw( 1, 1, "PLAYER: %c", currPlayer);
			refresh();
			turnCounter++;			
		}
	}
	clear();
	if(xcount==ocount)
	{
		mvprintw(row/2, (col/2)-2, "TIE!");
	}
	else
	{
		mvprintw(row/2, (col/2)-5, "PLAYER: %c WON!", (xcount<ocount)?'O':'X');
		
	}
	refresh();
	getch();
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

void master_work()
{
	char board[BOARD_SIZE][BOARD_SIZE];	
	init_board(board);
    display_menu(board);    
    printw( "Thanks for playing!" );
    getch();
    endwin();  
}

void slave_work()
{
	data buf;
	sldata slbuf;
	char filename[10];
	sprintf(filename, "%d",myrank); 
	MPI_Status status;
	while(true)
	{		
		MPI_Recv((void*)&buf, 1, mpi_msg_type, 0, 0, MPI_COMM_WORLD, &status);
		MPI_job_counter=buf.job_no;
		slbuf.ret_val=alpha_beta_pvs_r(buf.board, buf.depth, INT_MIN, INT_MAX, buf.player);
		slbuf.index = buf.index;
		MPI_Send(&slbuf, 1, mpi_slmsg_type, 0, 0, MPI_COMM_WORLD);
	}
}
int main(int argc, char **argv)
{	
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	create_struct();
	switch(myrank)
	{
		case 0:
			master_work();
			break;
		default:
			slave_work();
			break;
	}
	MPI_Finalize();
    return 0; 
}
