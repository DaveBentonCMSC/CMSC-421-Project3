#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/rwsem.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dave Benton <dbenton2@umbc.edu>");
MODULE_DESCRIPTION("Driver for Reversi");

#define BOARD_SIZE	67

static int numberOpens = 0; /* counts number of times module was opened*/
/*defBoard is used to reset the game anytime 00 is called*/
char defBoard[BOARD_SIZE] = "---------------------------OX------XO---------------------------\tX\n";
char board[BOARD_SIZE] = "---------------------------OX------XO---------------------------\tX\n";
/* Used just to make comparisons a little simpler*/
char X = 'X';
char O = 'O';
/* Used to hold which pieces the user picks and which the CPU gets */
char userPiece;
char comPiece;
/* Used to determine if it's the CPU or the user's turn*/
bool userMove = false;
/* Variables to hold responses read back to the driver program */
char gameResponse[BOARD_SIZE];
ssize_t gRespSize = -1;
/* Determines if there is a game going, used to display NOGAME*/
bool game = false;
/* Declares the lock */
static DECLARE_RWSEM(lock);

/* Function prototypes here */
static int	device_open(struct inode *, struct file *);
static int	device_release(struct inode *, struct file *);
static ssize_t	device_read(struct file *, char *, size_t, loff_t *);
static ssize_t 	device_write(struct file *, const char *, size_t, loff_t *);
static void	new_game(char piece);
static void	place_move(char col, char row);
static char	valid_move(int col, int row, char piece, char rets[]);
static void	cpu_move(void);
static void	flip_pieces(int col, int row, char piece, char *moves);
static void	user_pass(void);
static bool	check_winner(void);
static bool	check_winner_search(void);

/* Struct for file operations for the device */
const struct file_operations fops = 
{
	.owner = THIS_MODULE,
	.open = device_open,
	.read = device_read,
	.write = device_write,
	.release = device_release
};

/* Creates a struct for the device */
struct miscdevice reversiMisc =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = "reversi",
	.fops = &fops,
	.mode = 0666,
};


/* initialization function */
static int __init reversi_init(void)
{
	int err;
	err = misc_register(&reversiMisc); /* registers the device */
	if(err != 0) /* handles if there is an error when registering */
	{
		printk(KERN_ALERT "reversi failed to register a major number\n");
		return err;
	}	
	/* Displays to the kernel log that the device was initialized */
	printk(KERN_NOTICE "Reversi init :)\n");	
	return 0;
}

/* Exit function for the device */
static void __exit reversi_exit(void)
{
	misc_deregister(&reversiMisc); /* Deregisters the device */
	/* Displays to the kernel log that the device has been exited */
	printk(KERN_NOTICE "Reversi exit :(\n");
}


static int device_open(struct inode *inode, struct file *file)
{
	numberOpens++; /* Increments number of device opens */
	/* Displays to the kernel log how many times the device has been opened */
	printk(KERN_INFO "reversi: Device has been opened %d time(s)\n", numberOpens);
	return 0;
}


static ssize_t device_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	unsigned int ret; /* initializes the return for copy_to_user */
	down_read(&lock); /* locks the critical region */
	/* copies from the module to the user space buffer */
	ret = __copy_to_user(buffer, gameResponse, gRespSize);
	/* if there was an error copying, return it */
	if(ret < 0)
	{
		up_read(&lock); /* unlocks before returning */
		return ret;
	}
	up_read(&lock); /* unlocks before returning */
	return gRespSize;
}


static ssize_t device_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	/* initializes variables before locking */
	char cmd[7];
	int ret;
	/* locks the write critical region */
	down_write(&lock);
	/* copies from the user buffer to the module */
	ret = __copy_from_user(cmd, buffer, 7);	
	/* if user decides to start a game '00 X or O' */
	if((cmd[0] == '0' && cmd[1] == '0'))
	{
		/* if correctly entered after 02 */
		if(cmd[2] == ' ' && (cmd[3] == X || cmd[3] == O) && cmd[4] == '\n')
		{
			/* calls new game function */
			new_game(cmd[3]);
			/* copies response to variables used in read */
			strcpy(gameResponse, "OK\n");
			gRespSize = 7;
			
		}
		else /* if incorrectly entered after 02 */
		{
			strcpy(gameResponse, "INVFMT\n");
			gRespSize = 7;
		}
	} /* if user chooses to display the board */
	else if(cmd[0] == '0' && cmd[1] == '1' && cmd[2] == '\n')
	{
		/* copies board to the 'buffer' for read */
		memcpy(gameResponse, board,BOARD_SIZE);
		gRespSize = 67;
	}
	else if (game == true) /* if a game currently exists */
	{		
		/* if user enters '02' to make a move */
		if(cmd[0] == '0' && cmd[1] == '2')
		{
			/* if command is too long print INVFMT error */
			if (len > 7 || len < 7)
			{
				strcpy(gameResponse, "INVFMT\n");
				gRespSize = 7;
			} /* if command is correct */
			else if (cmd[2] == ' ' && cmd[4] == ' ')
			{
				/* calls function to place a move */
				place_move(cmd[3], cmd[5]);
			}
			else
			{	/* if right length but wrong format */
				strcpy(gameResponse, "INVFMT\n");
				gRespSize = 7;
			}
		} 	/* if user enters '03' for CPU move */
		else if(cmd[0] == '0' && cmd[1] == '3' && cmd[2] == '\n')
		{ 	/* calls function to make a CPU move */
			cpu_move();
		}
		else if(cmd[0] == '0' && cmd[1] == '4' && cmd[2] == '\n')
		{ 	/* calls function for user to pass their move */
			user_pass();
		}
		else
		{ 	/* anything else, responds with UNKCMD */
			strcpy(gameResponse, "UNKCMD\n");
			gRespSize = 7;
		}
	}
	else
	{ /* respons with NOGAME if a game has not been started or one has ended */
		strcpy(gameResponse, "NOGAME\n");
		gRespSize = 7;
	}
	/* unlocks the write before returning */
	up_write(&lock);
	return len;
}


static int device_release(struct inode *inodep, struct file *filep)
{ 	/* device release function, prints to kernel device has been closed */
	printk(KERN_INFO "reversi: Device closed");
	return 0;
}

static void new_game(char piece)
{
	userPiece = piece; /* sets user's piece */
	memcpy(board, defBoard, BOARD_SIZE); /* resets the board */
	if(piece == X) /* if user selected X, sets CPU's piece and sets user's move */
	{
		userMove = true;
		comPiece = O;
	}
	else /* if user selected ), sets CPU's piece and user's move */
	{
		userMove = false;
		comPiece = X;
	}
	game = true; /* sets game as 'being played' */
}

static void place_move(char col, char row)
{ 	/* initializes local variables */
	int col2, row2, moveLoc, ret1, ret2, i;
	/* this is an absolute mess but hey it works */
	char col3[2];
	char row3[2];
	bool win; /* holds true if someone has won */
	/* array of a return piece and pseudo booleans used as directions */
	char moves[9] = {'-', '0', '0', '0', '0', '0', '0', '0', '0'};	
	/* used to convert the chars into ints, needed to be null terminated */
	col3[0] = col;
	col3[1] = '\0';
	row3[0] = row;
	row3[1] = '\0';
	
	/* converts the chars to ints stored in row2 & col2 */
	ret1 = kstrtoint(row3, 0, &row2);
	ret2 = kstrtoint(col3, 0, &col2);
	/* if it could not convert */
	if (ret1 != 0 && ret2 != 0)
	{

		strcpy(gameResponse, "INVFMT\n");
		gRespSize = 7;
		return;
	}
	moveLoc = (8 * row2 + col2); /* location of the move converted to a single int*/
	if(userMove != false) /* if it is the user's turn */
	{
		if(board[moveLoc] == '-') /* if the selected location is empty */
		{
	/* was having issues with successive calls, used to reset the list */
			moves[0] = '-';
			for(i = 1; i < 9; i++)
			{
				moves[i] = '0';
			}
			/* checks if the move is valid */
			moves[0] = valid_move(col2, row2, userPiece, moves);
			/* if a valid move */
			if(moves[0] == userPiece)
			{
				board[moveLoc] = userPiece; /* sets the piece */
				board[65] = comPiece; /* sets next move on board */
				userMove = false; /* changes to CPU move */
				/* calls function to flip pieces */
				flip_pieces(col2, row2, userPiece, moves);
				/* checks for a winner */
				win = check_winner();
				if(win == false) /* if no winner */
				{
					strcpy(gameResponse, "OK\n");
					gRespSize = 3;
				}
				return;
			}
			else /* if location was not valid */
			{
				strcpy(gameResponse, "ILLMOVE\n");
				gRespSize = 8;
				return;
			}
		}
		else /* if location was already taken */
		{
			strcpy(gameResponse, "ILLMOVE\n");
			gRespSize = 8;
			return;
		}
	}
	else /* if it is not the user's turn */
	{
		strcpy(gameResponse, "OOT\n");
		gRespSize = 4;
		return;
	}
}


static char valid_move(int col, int row, char piece, char rets[])
{	/* initializes local variables */
	int dirs[8] = {0, 0, 0, 0, 0, 0, 0, 0}; /* list of directions to flip */
	int col2 = col; /* copies col, used to keep col as initial reference */
	int row2 = row; /* copies row, used to keep row as initial reference */
	char oppPiece = X; /* default piece opposite to piece */
	if(piece == X) /* if supplied piece is X */
	{
		oppPiece = O;
	}
	/* redundent but for good measure, if spot is not empty, invalid move */
	if(board[8 * row + col] != '-')
	{
		return '-';
	}
	/*obtains directions of possible valid moves, stores them in an array
	  I went with 0 and 1 just to keep the pseudo bool theme */
	if(row-1 >= 0 && board[8 * (row-1) + col] == oppPiece)
	{
		dirs[0] = 1;
	}
	if(row+1 < 8 && board[8 * (row+1) + col] == oppPiece)
	{
		dirs[1] = 1;
	}
	if(col-1 >= 0 && board[8 * row + (col-1)] == oppPiece)
	{
		dirs[2] = 1;
	}
	if(col+1 < 8 && board[8 * row + (col+1)] == oppPiece)
	{
		dirs[3] = 1;
	}
	if((row-1 >= 0 && col-1 >= 0) && board[8 * (row-1) + (col-1)] == oppPiece)
	{
		dirs[4] = 1;
	}
	if((row-1 >= 0 && col+1 < 8) && board[8 * (row-1) + (col+1)] == oppPiece)
	{
		dirs[5] = 1;
	}
	if((row+1 < 8 && col+1 < 8) && board[8 * (row+1) + (col+1)] == oppPiece)
	{
		dirs[6] = 1;
	}
	if((row+1 < 8 && col-1 >= 0) && board[8 * (row+1) + (col-1)] == oppPiece)
	{
		dirs[7] = 1;
	}
	
	/* Uses while loops to continue checking each possible direction
	   decided to go this way because I could avoid checking directions
	   that did not even have an opposing piece */
	while(dirs[0] == 1) /* while still checking direction */
	{
		row2 = row2-1; /* moves up a row */
		/* if in bounds and the location equals the current piece 
		   current piece signals we found a sandwiched piece(s) */
		if(row2 >= 0 && board[8 * (row2) + col] == piece)
		{	/* sets first spot in the array, used like a bool */
			rets[0] = piece;
			rets[1] = '1'; /* sets location as 'true', need to flip */
			row2 = row; /* resets row */
			dirs[0] = 0; /* ends loop */
		}
		/* if now out of bounds or found an empty spot first */
		if((row2 < 0) || board[8* (row2) + col] == '-')
		{	/* same deal as above */
			row2 = row;
			dirs[0] = 0;
		}
	}
	/* i'm not going to spell it out for every check, it does the same */
	while(dirs[1] == 1)
	{
		row2 = row2+1;
		if(row2 < 8 && board[8 * (row2) + col] == piece)
		{
			rets[0] = piece;
			rets[2] = '1';
			row2 = row;
			dirs[1] = 0;
		}
		if((row2 > 7) || board[8 * (row2) + col] == '-')
		{
			row2 = row;
			dirs[1] = 0;
		}
	}
	while(dirs[2] == 1)
	{
		col2 = col2-1;
		if(col2 >= 0 && board[8 * row + (col2)] == piece)
		{
			rets[0] = piece;
			rets[3] = '1';
			col2 = col;
			dirs[2] = 0;
		}
		if((col2 < 0) || board[8* row + (col2)] == '-')
		{
			col2 = col;
			dirs[2] = 0;
		}
	}
	while(dirs[3] == 1)
	{
		col2 = col2+1;
		if(col2 < 8 && board[8 * row + (col2)] == piece)
		{
			rets[0] = piece;
			rets[4] = '1';
			col2 = col;
			dirs[3] = 0;
		}
		if(col2 > 7 || board[8* row + (col2)] == '-')
		{
			col2 = col;
			dirs[3] = 0;
		}
	}
	while(dirs[4] == 1)
	{
		col2 = col2-1;
		row2 = row2-1;
		if(col2 >= 0 && row2 >= 0 && board[8 * (row2) + (col2)] == piece)
		{
			rets[0] = piece;
			rets[5] = '1';
			col2 = col;
			row2 = row;
			dirs[4] = 0;
		}
		if((col2 < 0 && row2 < 0) || board[8* (row2) + (col2)] == '-')
		{
			col2 = col;
			row2 = row;
			dirs[4] = 0;
		}
	}
	while(dirs[5] == 1)
	{
		col2 = col2+1;
		row2 = row2-1;
		if(col2 < 8 && row2 >= 0 && board[8 * (row2) + (col2)] == piece)
		{
			rets[0] = piece;
			rets[6] = '1';
			col2 = col;
			row2 = row;
			dirs[5] = 0;
		}
		if((col2 > 7 && row2 < 0) || board[8* (row2) + (col2)] == '-')
		{
			col2 = col;
			row2 = row;
			dirs[5] = 0;
		}
	}
	while(dirs[6] == 1)
	{
		col2 = col2+1;
		row2 = row2+1;
		if(col2 < 8 && row2 < 8 && board[8 * (row2) + (col2)] == piece)
		{
			rets[0] = piece;
			rets[7] = '1';
			col2 = col;
			row2 = row;
			dirs[6] = 0;
		}
		if((col2 > 7 && row2 > 7) || board[8* (row2) + (col2)] == '-')
		{
			col2 = col;
			row2 = row;
			dirs[6] = 0;
		}
	}
	while(dirs[7] == 1)
	{
		col2 = col2-1;
		row2 = row2+1;
		if(col2 >= 0 && row2 < 8 && board[8 * (row2) + (col2)] == piece)
		{
			rets[0] = piece;
			rets[8] = '1';
			col2 = col;
			row2 = row;
			dirs[7] = 0;
		}
		if((col2 < 0 && row2 > 7) || board[8* (row2) + (col2)] == '-')
		{
			col2 = col;
			row2 = row;
			dirs[7] = 0;
		}
	}
	return rets[0]; // if not a valid move, returns an empty space symbol at 0
	
}


static void cpu_move(void)
{	/* initializes local variables */
	int col, row, moveLoc, i;
	/* holds directions that need to be flipped */
	char moves[9] = {'-', '0', '0', '0', '0', '0', '0', '0', '0'};	
	bool win; /* holds true if win condition met */
	if(userMove == false) /* if it's not the user's move */
	{
		for(col = 0; col < 8; col++) /* iterates columns */
		{
			for(row = 0; row < 8; row++) /* iterates rows */
			{
				moveLoc = (8 * row + col); /* gets board location */
				/* slightly more redundancy, just good measure */
				if (board[moveLoc] == '-') /* if empty spot */
				{	/* like earlier, used to reset between calls */
					moves[0] = '-';
					for(i = i; i < 9; i++)
					{
						moves[i] = '0';
					}
					/* checks move for validity */
					moves[0] = valid_move(col, row, comPiece, moves);
					if(moves[0] == comPiece) /* if valid move */
					{	/* sets piece */
						board[moveLoc] = comPiece; 
						/* sets it to user's move */
						userMove = true; 
						/* sets next move on board */
						board[65] = userPiece;
						/* flips pieces */
						flip_pieces(col, row, comPiece, moves);
						/* checks for a winner */
						win = check_winner();
						if (win == false)
						{
							strcpy(gameResponse, "OK\n");
							gRespSize = 3;
						}
						return;
					}
				}	
			}
		}	
		/* fixes issue where if CPU had no move it would lock up */
		userMove = true; 
		board[65] = userPiece;
		strcpy(gameResponse, "OK\n");
		gRespSize = 3;
	}
	else /* if not the CPU's turn */
	{
		strcpy(gameResponse, "OOT\n");
		gRespSize = 4;
	}
}


static void flip_pieces(int col, int row, char piece, char *moves)
{	/* very similar to valid_move,I tried to combine them and just failed */
	/* copies to keep originals */
	int col2 = col;
	int row2 = row;
	char oppPiece = X; /* sets opposite piece default */
	if(piece == X) /* swaps opposite piece if flipped for X's */
	{
		oppPiece = O;
	}

	/* Uses while loops to continue checking each possible direction
	   again, I did it this way in order to only go in guaranteed directions */

	/* if upward needs to be flipped */
	while(*(moves+1) == '1')
	{
		row2 = row2-1; /* moves up a row */
		/* if in bounds and spot equals the piece that needs to be flipped */
		if(row2 >= 0 && board[8 * (row2) + col] == oppPiece)
		{
			board[8* (row2) + col] = piece; /* flips piece */
		}
		/* if we hit piece that doesn't need to be flipped, ends */
		if(board[8* (row2-1) + col] == piece)
		{
			row2 = row; /* resets row */
			*(moves+1) = '0'; /* ends loop */
		}
	}
	/* if downward needs to be flipped */
	while(*(moves+2) == '1')
	{	/* same as above for all directions */
		row2 = row2+1;
		if(row2 < 8 && board[8 * (row2) + col] == oppPiece)
		{
			board[8* (row2) + col] = piece;
		}
		if(board[8* (row2+1) + col] == piece)
		{
			row2 = row;
			*(moves+2) = '0';
		}
	}
	/* if left needs to be flipped */
	while(*(moves+3) == '1')
	{
		col2 = col2-1;
		if(col2 >= 0 && board[8 * row + (col2)] == oppPiece)
		{
			board[8* row + (col2)] = piece;
		}
		if(board[8* row + (col2-1)] == piece)
		{
			col2 = col;
			*(moves+3) = '0';
		}
	}
	/* if right needs to be flipped */
	while(*(moves+4) == '1')
	{
		col2 = col2+1;
		if(col2 < 8 && board[8 * row + (col2)] == oppPiece)
		{
			board[8 * row + (col2)] = piece;
		}
		if(board[8 * row + (col2+1)] == piece)
		{
			col2 = col;
			*(moves+4) = '0';
		}
	}
	/* if bottom left diag needs to be flipped */
	while(*(moves+5) == '1')
	{
		col2 = col2-1;
		row2 = row2-1;
		if(col2 >= 0 && row2 >= 0 && board[8 * (row2) + (col2)] == oppPiece)
		{
			board[8 * (row2) + (col2)] = piece;
		}
		if(board[8 * (row2-1) + (col2-1)] == piece)
		{
			col2 = col;
			row2 = row;
			*(moves+5) = '0';
		}
	}
	/* if top right diag needs to be flipped */
	while(*(moves+6) == '1')
	{
		col2 = col2+1;
		row2 = row2-1;
		if(col2 < 8 && row2 >= 0 && board[8 * (row2) + (col2)] == oppPiece)
		{
			board[8 * (row2) + (col2)] = piece;
		}
		if(board[8 * (row2-1) + (col2+1)] == piece)
		{
			col2 = col;
			row2 = row;
			*(moves+6) = '0';
		}
	}
	/* if bottom right diag needs to be flipped */
	while(*(moves+7) == '1')
	{
		col2 = col2+1;
		row2 = row2+1;
		if(col2 < 8 && row2 < 8 && board[8 * (row2) + (col2)] == oppPiece)
		{
			board[8 * (row2) + (col2)] = piece;
		}
		if(board[8 * (row2+1) + (col2+1)] == piece)
		{
			col2 = col;
			row2 = row;
			*(moves+7) = '0';
		}	
	}
	/* if top left diag needs to be flipped */
	while(*(moves+8) == '1')
	{
		col2 = col2-1;
		row2 = row2+1;
		if(col2 >= 0 && row2 + 1 < 8 && board[8 * (row2) + (col2)] == oppPiece)
		{
			board[8 * (row2) + (col2)] = piece;
		}
		if(board[8 * (row2+1) + (col2-1)] == piece)
		{
			col2 = col;
			row2 = row;
			*(moves+8) = '0';
		}
	}
}

static void user_pass(void)
{	/* initializes local variables, similar to above functions*/
	int col, row,i;
	/* holds any valid move directons */
	char moves[9] = {'-', '0', '0', '0', '0', '0', '0', '0', '0'};
	bool win; /* holds true if win condition met */
	if(userMove == true) /* if it in fact is the user's turn */
	{
		for(col = 0; col < 8; ++col) /* iterates columns */
		{
			for(row = 0; row < 8; ++row) /* iterates rows */
			{	/* like before, used to avoid issues with successive
				   calls */
				moves[0] = '-';
				for(i = 1; i < 9; i++)
				{
					moves[i] = '0';
				}
				/* checks move validity */
				moves[0] = valid_move(col, row, userPiece, moves);
				/* if a valid move found + redundancy check lol */
				if(moves[0] == userPiece && board[8 * row + col] == '-')
				{
					strcpy(gameResponse, "ILLMOVE\n");
					gRespSize = 8;
					return;
				}
			}
		}
		/* if no valid user moves found*/
		userMove = false; /* sets CPU's turn */
		board[65] = comPiece; /* sets next move on board */
		win = check_winner(); /* checks for a winner */
		if(win == false)
		{
			strcpy(gameResponse, "OK\n");
			gRespSize = 3;
		}
		return;
	}
	else /* if it is not the user's turn */
	{
		strcpy(gameResponse, "OOT\n");
		gRespSize = 4;
	}
}


static bool check_winner(void)
{	/* local variables to hold how many pieces each player has */
	int userCount = 0, cpuCount = 0, i;
	if(check_winner_search() == true) /* if no valid moves left */
	{
		for(i = 0; i < 64; i++) /*iterates over board */
		{
			if(board[i] == userPiece)
			{
				userCount++; /* counts number of user pieces */
			}
			if(board[i] == comPiece)
			{
				cpuCount++; /* counts number of CPU pieces */
			}
		}
		if(userCount > cpuCount) /* if user won */
		{
			strcpy(gameResponse, "WIN\n");
			gRespSize = 4;
		}
		else if (cpuCount > userCount)/* if CPU won */
		{
			strcpy(gameResponse, "LOSE\n");
			gRespSize = 5;
		}
		else /* if a tie */
		{
			strcpy(gameResponse, "TIE\n");
			gRespSize = 4;
		}
		game = false; /* sets no game in progress */
		return true; /* returns there was a win */
		
	}
	return false; /* returns no win */
}


static bool check_winner_search(void)
{	/* initializes local variables */
	char moves[9]; /* holds valid directions, useless here */
	char movesRet[2] = {'0', '0'}; /* index 0 is if a user move exists, 1 for CPU */
	char cpuMove, userMove; /* holds pseudo bool if valid move found */
	int col, row, j;
	/* iterates board to check for any valid moves at all */
	for(col = 0; col < 8; col++)
	{
		for(row = 0; row < 8; row++)
		{	/* yet another redundancy lol */
			if (board[8 * row + col] == '-')
			{	/* same as before, just resets to avoid issues */
				moves[0] = '-';
				for(j = 1; j < 9; j++)
				{
					moves[j] = '0';
				}
				/* checks if location is valid user move */
				userMove = valid_move(col, row, userPiece, moves);
				
				if(userMove == userPiece)
				{	/* if valid user move */
					moves[0] = '1';
				}
				/* resets again */
				moves[0] = '-';
				for(j = 1; j < 9; j++)
				{
					moves[j] = '0';
				}
				/* checks if location is valid CPU move */
				cpuMove = valid_move(col, row, comPiece, moves);
					
				if(cpuMove == comPiece)
				{	/* if valid CPU move */
					movesRet[1] = '1';
				}
			}
		}
	}
	/* if no valid moves for either player */
	if (movesRet[0] == '0' && movesRet[1] == '0')
	{
		return true; /* returns there is a win condition */
	}
	else
	{
		return false; /* no win condition */
	}
}
module_init(reversi_init);
module_exit(reversi_exit);
