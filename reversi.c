#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>
#include <time.h>
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
#define CORNER_WEIGHT 10

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

typedef int (*function)(char board[BOARD_SIZE][BOARD_SIZE], char player);

void make_move(char board[BOARD_SIZE][BOARD_SIZE], int x, int y, char player);
int find_possible_moves(char board[BOARD_SIZE][BOARD_SIZE], int moves[BOARD_SIZE * BOARD_SIZE][2], char currentPlayer);
int alpha_beta_r(char board[BOARD_SIZE][BOARD_SIZE], int depth, int a, int b, char player, bool is_opp);

//for ncurses
int row,col;
MPI_Datatype mpi_msg_type;
MPI_Datatype mpi_slmsg_type;
 
int MPI_job_counter=100;
int myrank;

int single_player = 1;
// if single_player => should heur_x == heur_o
int heur_x = 0;
int heur_o = 0;
 
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
	mvprintw(5, 3, "HS for %c: [%d]\n", player, count);
	return count;
}

int isCorner(int i, int j)
{
	return ((i == 0 && j == 0)
			|| (i == 0 && j == BOARD_SIZE - 1)
			|| (i == BOARD_SIZE - 1 && j == 0)
			|| (i == BOARD_SIZE - 1 && j == BOARD_SIZE - 1));
}

int mobility(char board[BOARD_SIZE][BOARD_SIZE], char player, int corner_weight, int countCorners)
{	
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
					pot_mob_opp += (isCorner(i, j) && countCorners) ? corner_weight : 1;
				if (pl_neighbour)
					pot_mob_pl += (isCorner(i, j) && countCorners) ? corner_weight : 1;
			}
		}
	result = mob_pl + pot_mob_pl - pot_mob_opp;	

	if (countCorners)
	{
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
	}
	
	mvprintw(4, 3, "Mobility %c: [%d], potential: player %c: [%d], opponent %c: [%d]\n", player, mob_pl, player, pot_mob_pl, opponent(player), pot_mob_opp);
	return result;
}

int heur_mob(char board[BOARD_SIZE][BOARD_SIZE], char player)
{
	int result = mobility(board, player, 1, 0);
	mvprintw(5, 3, "HM for %c: [%d]\n", player, result);
	return result;
}

int heur_mob_cor(char board[BOARD_SIZE][BOARD_SIZE], char player)
{
	int result = mobility(board, player, CORNER_WEIGHT, 1);
	mvprintw(5, 3, "HMC for %c: [%d]\n", player, result);
	return result;
}

int edges(char board[BOARD_SIZE][BOARD_SIZE], char player)
{
	int i;
	int player_edges = 0;
	for (i = 0; i < BOARD_SIZE; i++)
	{
		if (board[i][0] == player)
			player_edges++;
		if (board[i][BOARD_SIZE - 1] == player)
			player_edges++;
		if (board[0][i] == player)
			player_edges++;
		if (board[BOARD_SIZE - 1][i] == player)
			player_edges++;
	}
	return player_edges;
}

int heur_mob_cor_edg(char board[BOARD_SIZE][BOARD_SIZE], char player)
{
	int result = mobility(board, player, CORNER_WEIGHT, 1);
	int player_edges = edges(board, player), opponent_edges = edges(board, opponent(player));
	result += player_edges - opponent_edges;
	mvprintw(5, 3, "Edges: player %c: [%d], opponent %c: [%d]\n", player, player_edges, opponent(player), opponent_edges);
	mvprintw(6, 3, "HMCE for %c: [%d]\n", player, result);
	return result;
}

int stability(char board[BOARD_SIZE][BOARD_SIZE], char player)
{
	int i;
	int player_stable = 0;
	// right
	for (i = 0; i < BOARD_SIZE; i++)
	{
		if (board[i][0] == player)
			player_stable++;
		else
			break;
	}
	for (i = 0; i < BOARD_SIZE; i++)
	{
		if (board[i][BOARD_SIZE - 1] == player)
			player_stable++;
		else
			break;
	}
	// left
	for (i = BOARD_SIZE - 1; i >= 0; i--)
	{
		if (board[i][0] == player)
			player_stable++;
		else
			break;
	}
	for (i = BOARD_SIZE - 1; i >= 0; i--)
	{
		if (board[i][BOARD_SIZE - 1] == player)
			player_stable++;
		else
			break;
	}
	// down
	for (i = 1; i < BOARD_SIZE; i++)
	{
		if (board[0][i] == player)
			player_stable++;
		else
			break;
	}
	for (i = 1; i < BOARD_SIZE; i++)
	{
		if (board[BOARD_SIZE - 1][i] == player)
			player_stable++;
		else
			break;
	}
	// up
	for (i = BOARD_SIZE - 2; i >= 0; i--)
	{
		if (board[0][i] == player)
			player_stable++;
		else
			break;
	}
	for (i = BOARD_SIZE - 2; i >= 0; i--)
	{
		if (board[BOARD_SIZE - 1][i] == player)
			player_stable++;
		else
			break;
	}
	return player_stable;
}

int heur_mob_cor_edg_st(char board[BOARD_SIZE][BOARD_SIZE], char player)
{
	int result = heur_mob_cor_edg(board, player);
	int player_stable = stability(board, player), opponent_stable = stability(board, opponent(player));
	result += player_stable - opponent_stable;
	mvprintw(6, 3, "Stable: player %c: [%d], opponent %c: [%d]\n", player, player_stable, opponent(player), opponent_stable);
	mvprintw(7, 3, "HMCES for %c: [%d]\n", player, result);
	return result;
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

int heur_mob_cor_edg_st_time(char board[BOARD_SIZE][BOARD_SIZE], char player)
{
	int mob = mobility(board, player, CORNER_WEIGHT, 1);
	int player_edges = edges(board, player), opponent_edges = edges(board, opponent(player));
	int edges = player_edges - opponent_edges;
	int player_stable = stability(board, player), opponent_stable = stability(board, opponent(player));
	int stability = player_stable - opponent_stable;
	int turn, xscore = 0, yscore = 0;
	int result;
	
	count_stones(&xscore, &yscore, board);
	turn = xscore + yscore;
	
	if (turn <= 20)
	{
		result = (int)(1.5f * (float)mob + edges + 0.5f * (float)stability);
	}
	else if (turn <= 40)
	{
		result = mob + edges + stability;
	}
	else
	{
		result = (int)(0.5f * (float)mob + edges + 1.5f * (float)stability);
	}

	mvprintw(5, 3, "Edges: player %c: [%d], opponent %c: [%d]\n", player, player_edges, opponent(player), opponent_edges);
	mvprintw(6, 3, "Stable: player %c: [%d], opponent %c: [%d]\n", player, player_stable, opponent(player), opponent_stable);
	mvprintw(7, 3, "HMCEST for %c: [%d]\n", player, result);

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

static function heuristics[6] = { heur_sc, heur_mob, heur_mob_cor, heur_mob_cor_edg, heur_mob_cor_edg_st, heur_mob_cor_edg_st_time };

int alpha_beta_pvs_r(char board[BOARD_SIZE][BOARD_SIZE], int depth, int a, int b, char player, int heur_ind)
{
	if(depth==0)
		return heuristics[heur_ind](board, player);
	
	int pos_moves[BOARD_SIZE*BOARD_SIZE][2];
	int moves_c, i,score;
	moves_c=find_possible_moves(board, pos_moves, player);
	if(moves_c==0)
		return heuristics[heur_ind](board, player);
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
		score=-alpha_beta_pvs_r(temp, depth-1, -b, -a, opponent(player), heur_ind);
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

int pv_split(char board[BOARD_SIZE][BOARD_SIZE], int depth, int a, int b, char player, int heur_ind)
{
	if(depth==0)
		return heuristics[heur_ind](board, player);
	int pos_moves[BOARD_SIZE*BOARD_SIZE][2];
	int moves_c, i, score;
	moves_c=find_possible_moves(board, pos_moves, player);
	if(moves_c==0)
		return heuristics[heur_ind](board,player);
	char temp[BOARD_SIZE][BOARD_SIZE];
	int rank_c;
	data msg;
	sldata slmsg;
	MPI_Status status;
	MPI_Comm_size(MPI_COMM_WORLD, &rank_c); 
	
	copy_board(board, temp);
	make_move(temp, pos_moves[0][0], pos_moves[0][1], player);
	score=pv_split(temp, depth-1, a, b, opponent(player), heur_ind);
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
	int heur_ind = player == 'X' ? heur_x : heur_o;
	data msg;
	sldata slmsg;
	MPI_Status status;
	MPI_Comm_size(MPI_COMM_WORLD, &rank_c); 
	
	copy_board(board, temp);
	make_move(temp, pos_moves[0][0], pos_moves[0][1], player);
	score=pv_split(temp, ABDEPTH-1, a, b, opponent(player), heur_ind);
	a=score;
	index=0;
	msg.job_no=MPI_job_counter;
	
	//Begin parallel loop
	for(i=1; i<moves_c; i++)
	{
		copy_board(board, msg.board);
		make_move(msg.board, pos_moves[i][0], pos_moves[i][1], player);
		score=alpha_beta_pvs_r(temp, ABDEPTH-1, a, b, opponent(player), heur_ind);
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
	int i, rank_c, usrInput, turnCounter = 4, possibleMoves[BOARD_SIZE * BOARD_SIZE][2], moves, x, y;
	int xcount, ocount, move_was_possible;
	short clickedColor;	
	//char players[] = {'O', 'X'};
	char currPlayer = 'O';
	MEVENT event;
	time_t start, end;
	double time_diff;
	init_board(board);
	mousemask(BUTTON1_CLICKED , NULL); 
	MPI_Comm_size(MPI_COMM_WORLD, &rank_c); 
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
		if(turnCounter%2==0 && single_player)
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
			
			time(&start);
			int index=pv_split_master(board, possibleMoves, moves, currPlayer);
			time(&end);	
			time_diff = difftime(end, start);
			print_log("stats", "Heur no: %d, time: %d, depth: %d, no of processors: %d\n", (currPlayer == 'O' ? heur_o : heur_x), difftime, ABDEPTH, rank_c);			
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
			// debug
			heuristics[currPlayer == 'X' ? heur_x : heur_o](board, currPlayer);
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

	//mvprintw(1, (col/2)-5, "Time: %2lf seconds.", time_diff);

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
	int heur_ind = -1;
	while(true)
	{		
		MPI_Recv((void*)&buf, 1, mpi_msg_type, 0, 0, MPI_COMM_WORLD, &status);
		if (heur_ind == -1)
			heur_ind = buf.player == 'X' ? heur_x : heur_o;
		MPI_job_counter=buf.job_no;
		slbuf.ret_val=alpha_beta_pvs_r(buf.board, buf.depth, INT_MIN, INT_MAX, buf.player, heur_ind);
		slbuf.index = buf.index;
		MPI_Send(&slbuf, 1, mpi_slmsg_type, 0, 0, MPI_COMM_WORLD);
	}
}

void usage()
{
	fprintf(stderr,"USAGE: ./mpi_reversi.sh <heur O> <heur X>\n");
	fprintf(stderr,"Heur O and heur X are the heuristics for bots:\n");
	fprintf(stderr,"0 - Score\n");
	fprintf(stderr,"1 - Mobility\n");
	fprintf(stderr,"2 - Mobility, Corners\n");
	fprintf(stderr,"3 - Mobility, Corners, Edges\n");
	fprintf(stderr,"4 - Mobility, Corners, Edges, Stability\n");
	fprintf(stderr,"5 - Mobility, Corners, Edges, Stability, Time\n");
	fprintf(stderr,"-1 for one of them - single player mode\n");
	//{ heur_sc, heur_mob, heur_mob_cor, heur_mob_cor_edg, heur_mob_cor_edg_st, heur_mob_cor_edg_st_time };
	
}

void set_heuristics(char **argv)
{
	if(atoi(argv[1]) < 0)
	{
		single_player = 1;
		if(atoi(argv[2]) < 0)
		{
			printf("Heuristic for the bot not chosen, selecting the most complex one");
			heur_o = heur_x = 5;
		}
		else
			heur_o = heur_x = atoi(argv[2]);
	}
	else if(atoi(argv[2]) < 0)
	{
		single_player = 1;
		heur_o = heur_x = atoi(argv[1]);
	}
	else
	{
		single_player = 0;
		heur_o = atoi(argv[1]);
		heur_x = atoi(argv[2]);
	}
}

int main(int argc, char **argv)
{	
	if(argc != 3 || atoi(argv[1]) > 5 || atoi(argv[2]) > 5)
	{
		usage();
		return EXIT_FAILURE;
	}
	set_heuristics(argv);
	
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
