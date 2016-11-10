#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <curses.h>
#include <sys/wait.h>
#define	MAXLINE	4096	/* max text line length */


#define	SERV_FIFO	"/tmp/fifo.serv"

int connection_fifo, key_fifo;
pid_t	pid;
char fifoname[MAXLINE];
char lost;
char head, body;
int length;

void print_score();

void sig_usr(int signal)
{
  lost=1;
  endwin();	
  print_score();
  close(key_fifo);		
  unlink(fifoname);
  exit(0);
}

void request_connection()
{
  char buff[MAXLINE];

  /* open FIFO to server and write PID and pathname to FIFO */
  connection_fifo = open(SERV_FIFO, O_WRONLY, 0);
  snprintf(buff, sizeof(buff), "%ld %d %c %c", (long) pid,length,head,body);
  write(connection_fifo, buff, strlen(buff));	
  close(connection_fifo);

}	
void create_fifo()
{
  snprintf(fifoname, sizeof(fifoname), "/tmp/fifo.%ld", (long) pid);
  
  //if ((mkfifo(fifoname, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) < 0) && (errno != EEXIST))
  if ((mkfifo(fifoname, 0666) < 0) && (errno != EEXIST))
    printf("can't create %s", fifoname);  
}

void read_key(int temp)
{

  switch(temp)
  {
#ifdef KEY_LEFT
     case KEY_LEFT:
#endif
     case 'h':
       write(key_fifo,"h", 1); break;

#ifdef KEY_DOWN
     case KEY_DOWN:
#endif
     case 'j':
       write(key_fifo,"j", 1); break;

#ifdef KEY_UP
     case KEY_UP:
#endif
     case 'k':
       write(key_fifo,"k", 1);break;

#ifdef KEY_RIGHT
     case KEY_RIGHT:
#endif
     case 'l':
       write(key_fifo,"l", 1);break;
     default: 
       return;
  }	
	
}		

void print_score()
{
  int readfifo,score,n;
  char exitfifoname[60],buff[10];

  snprintf(exitfifoname, sizeof(exitfifoname), "/tmp/fifo.%ld.end", (long) pid);
  readfifo=open(exitfifoname, O_RDONLY, 0);	
  n=read(readfifo, buff, 10);
  if (buff[n-1] == '\n')
    n--;			/* delete newline from readline() */
    buff[n] = '\0';		/* null terminate buffer */
  score=atol(buff);
  endwin();
  printf("You lost, your score was: %d\n",score);
  close(readfifo);
  unlink(exitfifoname);  
}

void check_server_response()
{
  int readfifo;
  char checkfifoname[60],buff[20];

  snprintf(checkfifoname, sizeof(checkfifoname), "/tmp/fifo.%ld.not", (long) pid);
  readfifo=open(checkfifoname, O_RDONLY, 0);	
  read(readfifo, buff, 20);
  close(readfifo);
  unlink(checkfifoname);  
  if(strcmp(buff,"SUCCESS"))
  {
   printf("Success connecting!\n");
   return; 
  }
  else
  {
	printf("Error connecting to server!\n");  
	close(key_fifo);		
    unlink(fifoname);
    exit(0);
  } 	 
}   
   	

int main(int argc, char **argv)
{

  if(argc!=4)
  {
    printf("Usage worms length head body\n");
    return 0; 	   
  }	 
    
  length=atoi(argv[1]);
  head=(char)argv[2][0];
  body=(char)argv[3][0];  
  lost=0;
  //signal for SIGUSR1
  if (signal(SIGUSR1, sig_usr) == SIG_ERR)
    printf("signal error for SIGUSR1");  

  pid = getpid();	
  create_fifo();
  request_connection();
  check_server_response();
  initscr();
#ifdef KEY_LEFT
  keypad(stdscr, TRUE);
#endif
  key_fifo = open(fifoname, O_WRONLY, 0);
  while(!lost)	
    read_key(getch());
   
 //We lost print score, close files and exit
  print_score();
  close(key_fifo);		
  unlink(fifoname);
  exit(0);
}
