#include <sys/cdefs.h>
#include <ctype.h>
#include <curses.h>
#include <err.h>
#include <signal.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

#define newlink() (struct body *) malloc(sizeof (struct body));
#define LENGTH 7
#define RUNLEN 8
#define MAX_NO_PLAYERS 4
#define	SERV_FIFO	"/tmp/fifo.serv"
#define	MAXLINE	90	/* max text line length */

WINDOW *tv;
WINDOW *stw;
struct body {
      int x;
      int y;
      struct body *prev;
      struct body *next;
} goody;

struct worm{
	struct body *head;
	struct body *tail;
	char HEAD;
	char BODY;
	int growing;
	int running;
	int slow;
	int score;
	int start_len;
	int visible_len;
	int lastch;
	char exists;
};

struct player
{
 struct worm player_worm;
 long pid;		
};

char outbuf[BUFSIZ],exit_request;
int current_players,readfifo,dummyfd;
struct player players[MAX_NO_PLAYERS];
int available_players[MAX_NO_PLAYERS];
int plyr_index;


void  crash(struct worm *);
void  display(const struct body *, char);
void  life(struct worm *,int);
void  newpos(struct worm *,struct body *);
void  process(struct worm *,int, int);
void  prize(void);
int   rnd(int);
void  setup(void);
void *worm_thread(void *arg);
void init_players();
void destroy_worm(struct worm*);
void sig_int(int sig);
int compare (const void* p1, const void* p2);
int front_player();
void back_player(int  i);
void sort_players();
void init_list();

int main(int argc, char **argv) {
	 /*threads*/
	  pthread_t threads[MAX_NO_PLAYERS];
	  void *thread_result;
	  current_players=0;
	  int res, writefifo,fd,i,current_player;
      char	*ptr, buff[MAXLINE],*start,*end; 
	  ssize_t	n;
	  long pid;
      /* Revoke setgid privileges */
      setregid(getgid(), getgid());
      exit_request=0;
     //signal for SIGINT
     if (signal(SIGINT, sig_int) == SIG_ERR)
       printf("signal error for SIGINT"); 
       
      setbuf(stdout, outbuf);
      srand(getpid()); 
      initscr();
      cbreak();
      noecho();
#ifdef KEY_LEFT
      keypad(stdscr, TRUE);
#endif
      clear();
      if (COLS < 18 || LINES < 5) {
            /*
             * Insufficient room for the line with " Worm" and the
             * score if fewer than 18 columns; insufficient room for
             * anything much if fewer than 5 lines.
             */
            endwin();
            errx(1, "screen too small");
      }
      stw = newwin(1, COLS-1, 0, 0);
      tv = newwin(LINES-1, COLS-1, 1, 0);
      box(tv, '*', '*');
      scrollok(tv, FALSE);
      scrollok(stw, FALSE);
       
      wmove(stw, 0, 0);
      wprintw(stw, " Worm");
      refresh();
      wrefresh(stw);
      wrefresh(tv);
      
      init_players();
      init_list();
      plyr_index=MAX_NO_PLAYERS;	
      //Make and open FIFO for request
      if ((mkfifo(SERV_FIFO, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) < 0) && (errno != EEXIST))
	  {
        perror("Server FIFO creation failed");
        exit(EXIT_FAILURE);
	  } 	
	 
	  readfifo = open(SERV_FIFO, O_RDONLY, 0);
	  dummyfd = open(SERV_FIFO, O_WRONLY, 0);		/* never used */
	
/*********************************MAIN LOOP*********************************************/		
      while ( (n = read(readfifo, buff, MAXLINE)) > 0) 
      {
		
		
		if (buff[n-1] == '\n')
			n--;			/* delete newline from readline() */
		buff[n] = '\0';		/* null terminate pathname */
		//get PID
		start=buff;
		end=strchr(start,' ');
		*end++='\0';
		
		//No room for new players :(
		if(current_players==MAX_NO_PLAYERS)
		{
		  notify_client(atol(start),"FAILED"); 
		  continue;
	    }	
	    
	    current_player=front_player();
	    
		players[current_player].pid = atol(start); 
	    //get length
		start=end;
		end=strchr(start,' ');
		*end++='\0';
		players[current_player].player_worm.start_len=atol(start);
		//get head
		start=end;
		players[current_player].player_worm.HEAD=*start++;
		//get body
		players[current_player].player_worm.BODY=*(++start);
		
		//Create a thread to handle the user key input
        res=pthread_create(&threads[current_player], NULL, worm_thread,(void*)current_player);
        
        
        if (res != 0) 
        {    
          perror("Thread creation failed");
          //We notify the client that the worm was not created
          notify_client(players[current_player].pid,"FAILED");  
         // exit(EXIT_FAILURE);
        } 
		current_players++; 	 
      }  
      
    for(i=0;i<MAX_NO_PLAYERS;i++)
      if(players[i].player_worm.exists)
	    finish_player(i);
    unlink(SERV_FIFO);  
    exit(0);       

}
int compare (const void * a, const void * b)
{
  return ( *(int*)a - *(int*)b );
}
void init_list()
{
 int i;
 for(i=0;i<MAX_NO_PLAYERS;i++)
 	available_players[i]=i;
}	
int front_player()
{
 int i;
 int ret=available_players[0];
 plyr_index--;
 for(i=0;i<plyr_index;i++)
   available_players[i]=available_players[i+1];	
 return ret;	
}
void back_player(int  i)
{
  available_players[plyr_index++]=i;
  qsort(available_players,plyr_index,sizeof(int),compare);

}	
	
void sig_int(int sig)
{
  int i;
  exit_request=1;	
  clear();
  endwin();	
  for(i=0;i<MAX_NO_PLAYERS;i++)
    if(players[i].player_worm.exists)
	  finish_player(i,0);   
  close(readfifo);	
  close(dummyfd);  
  unlink(SERV_FIFO);  
  exit(0); 
    
}	
void notify_client(long pid, char* status)
{
  int writefifo,dummyfd;
  char fifoname[MAXLINE];
  
  snprintf(fifoname, sizeof(fifoname), "/tmp/fifo.%ld.not", pid);
  
  if ((mkfifo(fifoname, 0666) < 0) && (errno != EEXIST))
    printf("can't create %s", fifoname);  
  writefifo = open(fifoname, O_WRONLY, 0);
  write(writefifo,status,strlen(status));  	
}	
void init_player(int i)
{
  players[i].player_worm.growing = 0; 
  players[i].player_worm.running = 0;
  players[i].player_worm.score = 0;
  players[i].player_worm.slow = (baudrate() <= 1200);
  players[i].player_worm.exists=0;
	
}	
void init_players()
{
  int i;
  for(i=0;i<MAX_NO_PLAYERS;i++)
	  init_player(i);

}	
void finish_player(int player,int flag)
{ 
  int writefifo,dummyfd;
  char fifoname[MAXLINE],buff[20];
 if(flag)
 {
  wmove(stw, 0, 12*player);
  wprintw(stw, "            ", player);
  wrefresh(stw); 
 }
  kill((pid_t)players[player].pid,SIGUSR1); //mandamos una señal al cliente para que termine
  
  snprintf(fifoname, sizeof(fifoname), "/tmp/fifo.%ld.end", players[player].pid);
  
  if ((mkfifo(fifoname, 0666) < 0) && (errno != EEXIST))
    printf("can't create %s", fifoname);  
  writefifo = open(fifoname, O_WRONLY, 0);
  snprintf(buff, sizeof(buff), "%d",players[player].player_worm.score);
  write(writefifo,buff,strlen(buff));  	
}
	
void destroy_worm(struct worm* wrm)
{
  struct body *curr,*next;
  curr=wrm->tail;
  while(curr!=NULL)
  {
	display(curr,' ');
	next=curr->next;
	free(curr);
	curr=next;	  
  }   	
  wrm->exists=0;	
}	
void life(struct worm *W,int ypos){
      struct body *bp, *np;
      int i, j = 1;

      np = NULL;
      W->head = newlink();
      if (W->head == NULL)
            err(1, NULL);
      
      W->head->x = W->start_len % (COLS-5) + 2;
      W->head->y = LINES / ypos;
      W->head->next = NULL;
      display(W->head, W->HEAD);
      for (i = 0, bp = W->head; i < W->start_len; i++, bp = np) {
            np = newlink();
            if (np == NULL)
                  err(1, NULL);
            np->next = bp;
            bp->prev = np;
            if (((bp->x <= 2) && (j == 1)) || ((bp->x >= COLS-4) && (j == -1))) {
                  j *= -1;
                  np->x = bp->x;
                  np->y = bp->y + 1;
            } else {
                  np->x = bp->x - j;
                  np->y = bp->y;
            }
            display(np, W->BODY);
      }
      W->tail = np;
      W->tail->prev = NULL;
      W->visible_len = W->start_len + 1;
}

void display(const struct body *pos, char chr){
      wmove(tv, pos->y, pos->x);
      waddch(tv, chr);
}


int rnd(int range){
      return abs((rand()>>5)+(rand()>>5)) % range;
}

void newpos(struct worm *W,struct body * bp){
      if (W != NULL && W->visible_len == (LINES-3) * (COLS-3) - 1) {
            endwin();

            printf("\nYou won!\n");
            printf("Your final score was %d\n\n", W->score);
            exit(0);
      }
      do {
            bp->y = rnd(LINES-3)+ 1;
            bp->x = rnd(COLS-3) + 1;
            wmove(tv, bp->y, bp->x);
      } while(winch(tv) != ' ');
}

void prize(void){
      int value;
      value = rnd(9) + 1;
      newpos(NULL, &goody);
      waddch(tv, value+'0');
      wrefresh(tv);
}

void process(struct worm *W,int ch, int n_player){
      int x,y;
      struct body *nh;

      x = W->head->x;
      y = W->head->y;
      switch(ch)
      {
#ifdef KEY_LEFT
            case KEY_LEFT:
#endif
            case 'h':
                  x--; break;

#ifdef KEY_DOWN
            case KEY_DOWN:
#endif
            case 'j':
                  y++; break;

#ifdef KEY_UP
            case KEY_UP:
#endif
            case 'k':
                  y--; break;

#ifdef KEY_RIGHT
            case KEY_RIGHT:
#endif
            case 'l':
                  x++; break;

            default: 
                     return;
      }
      W->lastch = ch;
      if (W->growing == 0)
      {
            display(W->tail, ' ');
            W->tail->next->prev = NULL;
            nh = W->tail->next;
            free(W->tail);
            W->tail = nh;
            W->visible_len--;
      }
      else W->growing--;
      display(W->head, W->BODY);
      wmove(tv, y, x);
      if (isdigit(ch = winch(tv)))
      {
            W->growing += ch-'0';
            prize();
            W->score += W->growing;
            W->running = 0;
            wmove(stw, 0, 12*n_player);
            wprintw(stw, "PLYR %d:%3d  ", n_player, W->score);
            wrefresh(stw);
      }
      else if(ch != ' ')
      { 
	    crash(W);
	    if (!(W->slow && W->running))
        {
          wmove(tv, W->head->y, W->head->x);
          wrefresh(tv);
        }   
	    return; 
	  }
      nh = newlink();
      if (nh == NULL)
            err(1, NULL);
      nh->next = NULL;
      nh->prev = W->head;
      W->head->next = nh;
      nh->y = y;
      nh->x = x;
      display(nh, W->HEAD);
      W->head = nh;
      W->visible_len++;
      if (!(W->slow && W->running))
      {
            wmove(tv, W->head->y, W->head->x);
            wrefresh(tv);
      }
      
}

void crash(struct worm *W){
	 /*endwin();
	 printf("\nWell, you ran into something and the game is over.\n");
     printf("Your final score was %d\n\n", W->score);
     exit(0);	*/
     destroy_worm(W);			
}

void setup(){
      clear();
      refresh();
      touchwin(stw);
      wrefresh(stw);
      touchwin(tv);
      wrefresh(tv);
}
void *worm_thread(void *arg)
{
  int player=(int)arg;
  ssize_t	n;
  char buff[2],fifoname[MAXLINE];
  int readfifo,dummyfd;
  
  if(player>=MAX_NO_PLAYERS)
  {
	printf("Thread parameter error! \n");
	//We notify the client that the worm was not created
	notify_client(players[player].pid,"FAILED");
	pthread_exit(NULL);
  }  
  //We notify the client that the worm was created
  notify_client(players[player].pid,"SUCCESS");
  
  init_player(player);
  snprintf(fifoname, sizeof(fifoname), "/tmp/fifo.%ld", players[player].pid);
  readfifo = open(fifoname, O_RDONLY, 0);
  dummyfd = open(fifoname, O_WRONLY, 0);		/* never used */ 
  
  life(&players[player].player_worm,player+2);	
  players[player].player_worm.exists=1;
  prize();          /* Put up a goal */
  while((n = read(readfifo, buff, 1)) > 0)
  {
		if (players[player].player_worm.running)
		{
			  players[player].player_worm.running--;
			  process(&players[player].player_worm,
			           players[player].player_worm.lastch,player);
		}
		else
		{
			fflush(stdout);
			process(&players[player].player_worm,buff[0],player);
		}
		if(!players[player].player_worm.exists)
		  break;
  }	
    finish_player(player,1);
    current_players--;
    back_player(player);
	pthread_exit(NULL);
}	
