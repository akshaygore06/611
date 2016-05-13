/**
 *  Programmer : Akshay Gore
 *  Program Name : Goldchase
 *  Subject : CSCI 611
 */

#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <errno.h>
#include "goldchase.h"
#include "Map.h"
#include <fstream>
#include <string>
#include <signal.h>
#include <sys/types.h>
#include <mqueue.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h> //for read/write
#include <string.h> //for memset
#include <stdio.h> //for fprintf, stderr, etc.
#include <stdlib.h> //for exit
#include "fancyRW.h"
#include <cassert>
#include <stdarg.h>

using namespace std;

//The macro that contains the full pathname to the named pipe
//#define NAMEDPIPE "/home/tgibson/studentwork/gore/err_out"
#define NAMEDPIPE "/home/akshay/private_git_611/611/mypipe"
//The macro for writing debugging messages to a named pipe
#define DEBUG(msg, ...) do{sprintf(debugStr,msg, ##__VA_ARGS__); WRITE(debugFD,debugStr, strlen(debugStr));}while(0)

//used by DEBUG macro
char debugStr[100];
int debugFD;


/** GameBoard Structure for the Goldchase */

struct GameBoard {
  int rows; //4 bytes
  int cols; //4 bytes
  pid_t player_pid[5];
  unsigned char player;
  int daemonID;
  unsigned char map[0];
};

/** Global Variables **/
Map *mapptr;
mqd_t readqueue_fd; //message queue file descriptor
struct mq_attr mq_attributes;
string mqueue_name[5];
bool sig=false;
GameBoard* goldmap;
//unsigned char sb;
int client_shared_mem;
int fd;
unsigned char* local_mapcopy;
sem_t* GameBoard_Sem;
int pipefd[2];
int NumberOfRows = -1;
int NumberOfColumns = -1;
unsigned char byte,listen_var,listen_var1;
int sockfd,new_sockfd;
int status;
/** Function Declaration */
int insertGold(unsigned char*,int ,int ,int);
int insertPlayer(unsigned char*,unsigned char ,int ,int);
void handle_interrupt(int);
void read_message(int);
void clean_up(int);
void readQueue(string mqueue_name);
void writeQueue(string mqueue_name,string strMessage);
void refreshScreen(GameBoard* goldmap);
void create_server_daemon();
void sigusr1_handeler(int z);
void sigusr2_handeler(int z);
void sighup_handeler(int z);
void client_deamon(string);
void continuous_listen();
//unsigned char	SockPlayer1;
unsigned char c;
int main(int argc, char* argv[])
{
  debugFD =  open(NAMEDPIPE,O_WRONLY);
  assert(debugFD!=-1); //go no further if we couldn't open the named pipe

  //string client_ip = "localhost" ;
  DEBUG("I am here-01\n");

  /** Variable Declaration */
  ifstream inputFile;
  string Line,CompleteMapString= "";
  int No_Of_Gold,mapSize;
  bool firstline = true;
  bool collen_flag = true;

  char* theMine;
  const char* ptr;
  unsigned char* mp;
  unsigned char* client_mp;
  int player_position;
  unsigned char current_player;
  pid_t current_pid;
  string msgString;


  mqueue_name[0] = "/agore_player0_mq";
  mqueue_name[1] = "/agore_player1_mq";
  mqueue_name[2] = "/agore_player2_mq";
  mqueue_name[3] = "/agore_player3_mq";
  mqueue_name[4] = "/agore_player4_mq";

  /** Signal */
  struct sigaction sigactionObject;
  sigactionObject.sa_handler=handle_interrupt;
  sigemptyset(&sigactionObject.sa_mask);
  sigactionObject.sa_flags=0;
  sigactionObject.sa_restorer=NULL;

  //beginning of main, game process handles SIGUSR1 as handle_interrupt
  sigaction(SIGUSR1, &sigactionObject, NULL);
  struct sigaction exit_handler;
  exit_handler.sa_handler=clean_up;
  sigemptyset(&exit_handler.sa_mask);
  exit_handler.sa_flags=0;
  sigaction(SIGINT, &exit_handler, NULL);
  //sigaction(SIGHUP, &exit_handler, NULL);
  sigaction(SIGTERM, &exit_handler, NULL);

  //make sure we can handle the SIGUSR2
  //message when the message queue
  //notification sends the signal
  struct sigaction action_to_take;
  action_to_take.sa_handler= read_message;	//handle with this function interrupt
  sigemptyset(&action_to_take.sa_mask);//zero out the mask (allow any signal to)
  action_to_take.sa_flags=0;//tell how to handle SIGINT
  sigaction(SIGUSR2, &action_to_take, NULL);// struct mq_attr mq_attributes;
  mq_attributes.mq_flags=0;
  mq_attributes.mq_maxmsg=10;
  mq_attributes.mq_msgsize=120;

  if(argc == 2)
  {
    GameBoard_Sem = sem_open("/GameBoard_Sem",O_CREAT|O_RDWR|O_EXCL,S_IROTH|S_IWOTH|S_IRGRP|S_IWGRP|S_IRUSR|S_IWUSR,1);

    if( GameBoard_Sem != SEM_FAILED)
    {
      client_deamon(argv[1]);
    }
  }

  //if(argc != 2)
  //{
  /** Reading map file */
  inputFile.open("mymap.txt");
  while(inputFile != '\0')
  {
    if(firstline == true)
    {
      getline(inputFile,Line);
      No_Of_Gold = stoi(Line);
      firstline = false;
    }
    else
    {
      getline(inputFile,Line);
      CompleteMapString += Line;
      NumberOfRows++;
      //MAP_ROW++;
      if(collen_flag == true)
      {
        NumberOfColumns = Line.length();
        collen_flag =  false;
      }
    }
  }
  inputFile.close();

  mapSize = NumberOfRows * NumberOfColumns;
  //}
  /*
     argv checking
     */


  /** Creating semaphore */


  GameBoard_Sem = sem_open("/GameBoard_Sem",O_RDWR|O_EXCL,S_IROTH|S_IWOTH|S_IRGRP|S_IWGRP|S_IRUSR|S_IWUSR,1);

  if( GameBoard_Sem == SEM_FAILED) /** Code for First Player */
  {
    GameBoard_Sem = sem_open("/GameBoard_Sem",O_CREAT|O_EXCL|O_RDWR ,S_IROTH|S_IWOTH|S_IRGRP|S_IWGRP|S_IRUSR|S_IWUSR,1);///create a semaphore

    /** Code for Shared memory creation*/

    sem_wait(GameBoard_Sem);
    fd = shm_open("/GameBoard_Mem", O_CREAT|O_EXCL|O_RDWR , S_IRUSR | S_IWUSR);
    goldmap = (GameBoard*)mmap(NULL, NumberOfRows*NumberOfColumns+sizeof(GameBoard), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    if(ftruncate(fd,mapSize + sizeof(GameBoard)) != 0 )
    {
      perror("Truncate Memory");
    }

    if(goldmap ==  MAP_FAILED)
    {
      perror("MMaP Error :");
    }

    goldmap->rows = NumberOfRows;
    goldmap->cols = NumberOfColumns;
    goldmap->daemonID = 0;
    current_player = G_PLR0;


    //Convert the ASCII bytes into bit fields drawn from goldchase.h
    theMine = &CompleteMapString[0];
    ptr = theMine;
    mp = goldmap->map;
    while(*ptr!='\0')
    {
      if(*ptr==' ')      *mp=0;
      else if(*ptr=='*') *mp=G_WALL; //A wall
      ++ptr;
      ++mp;
    }

    /** Insert GOLD into the shared memory */
    insertGold(goldmap->map,No_Of_Gold,NumberOfRows,NumberOfColumns);

    /** Insert First player into the shared memory */
    player_position = insertPlayer(goldmap->map,current_player,NumberOfRows,NumberOfColumns);

    for(int i = 0; i < 5; i++)
    {
      goldmap->player_pid[i] = 0;
    }

    goldmap->player_pid[0] = getpid();

    readQueue(mqueue_name[0]);   // read mqueue of first player
    sem_post(GameBoard_Sem);

  }
  else  /** Code for Subsequent Players*/
  {
    GameBoard_Sem = sem_open("/GameBoard_Sem",O_RDWR|O_EXCL,S_IROTH|S_IWOTH|S_IRGRP|S_IWGRP|S_IRUSR|S_IWUSR,1);

    int fdiscriptor = shm_open("/GameBoard_Mem", O_RDWR|O_EXCL , S_IRUSR | S_IWUSR);
    if(fdiscriptor == -1)
    {
      cerr<<"second FD "<<endl;
    }

    // read shared map
    int map_rows, map_columns ;
    READ(fdiscriptor, &map_rows, sizeof(int));
    READ(fdiscriptor, &map_columns, sizeof(int));

    goldmap = (GameBoard*)mmap(NULL, map_rows*map_columns+sizeof(GameBoard), PROT_READ|PROT_WRITE, MAP_SHARED, fdiscriptor, 0);
    if(goldmap == MAP_FAILED )
    {
      cerr<<"MAP Reading for Else Part "<<endl;
    }

    NumberOfColumns = goldmap->cols;
    NumberOfRows = goldmap->rows;

    // setting players pid
    int iter=0;
    int plr[5] = {G_PLR0,G_PLR1,G_PLR2,G_PLR3,G_PLR4};

    for(iter = 0 ; iter < 5;iter++)
    {
      if( goldmap->player_pid[iter] == 0)
      {
        current_player = plr[iter];
        goldmap->player_pid[iter] = getpid();
        //		goldmap->daemonID = 0;
        readQueue(mqueue_name[iter]);
        break;
      }
    }
    if(iter == 5)
    {
      cout << "No more players allowed" <<endl;
      return 0;
    }
    player_position = insertPlayer(goldmap->map,current_player,NumberOfRows,NumberOfColumns);
    DEBUG("before sighup player insert\n");
    kill(goldmap->daemonID,SIGHUP); // need to check

  }


  sem_wait(GameBoard_Sem);
  goldmap->player |= current_player;
  sem_post(GameBoard_Sem);

  //goldmap->daemonID =0;

  Map goldMine(goldmap->map,goldmap->rows,goldmap->cols);


  mapptr = &goldMine;

  refreshScreen(goldmap);


  /** Code of Movement*/


  int currentColumn,currentRow;
  int a=0;
  bool running_flag =  true,realGoldFound=false, sendSignal = false;

  if(goldmap->daemonID == 0)
  {
    DEBUG("server created\n");
    create_server_daemon();

  }
  else
  {
    DEBUG("NOT server created\n");
    kill(goldmap->daemonID,SIGHUP);
  }


  while(running_flag && sig==false)
  {
    a = goldMine.getKey();
    currentRow = player_position / NumberOfColumns;
    currentColumn = player_position % NumberOfColumns;

    if(currentColumn == 0)
    {
      currentColumn = NumberOfColumns;
    }

    if( a == 104 ) //  'h' Key for Left Movement
    {

      if(realGoldFound == true && currentColumn-1 == 0)
      {
        sem_wait(GameBoard_Sem);
        goldmap->map[player_position] &= ~current_player;
        sem_post(GameBoard_Sem);
        //	kill(goldmap->daemonID,SIGUSR1);
        break;
      }
      if(goldmap->map[player_position-1] != G_WALL )
      {
        goldmap->map[player_position] &= ~current_player;
        player_position = player_position -1;
        goldmap->map[player_position]  |= current_player;
        goldMine.drawMap();
        sendSignal = true;

      }

      //kill(goldmap->daemonID,SIGUSR1);
    }
    else if( a == 108 ) /// 'l' Key for Right Movement
    {
      if(realGoldFound == true && currentColumn == goldmap->cols -1)
      {
        sem_wait(GameBoard_Sem);
        goldmap->map[player_position] &= ~current_player;
        sem_post(GameBoard_Sem);
        //	kill(goldmap->daemonID,SIGUSR1);
        break;
      }
      if(goldmap->map[player_position+1] != G_WALL )//| currentColumn != 0 )
      {
        goldmap->map[player_position] &= ~current_player;
        player_position = player_position+1;
        goldmap->map[player_position]  |= current_player;
        goldMine.drawMap();
        sendSignal = true;


      }
      //	kill(goldmap->daemonID,SIGUSR1);
    }
    else if( a == 106 ) // 'j' Key for Down Movement
    {
      if(realGoldFound == true && currentRow >= NumberOfRows-1 )
      {
        sem_wait(GameBoard_Sem);
        goldmap->map[player_position] &= ~current_player;
        sem_post(GameBoard_Sem);
        //	kill(goldmap->daemonID,SIGUSR1);
        break;
      }
      if(goldmap->map[player_position+NumberOfColumns] != G_WALL  && currentRow < NumberOfRows-1 )
      {
        goldmap->map[player_position] &= ~current_player;
        player_position = player_position+NumberOfColumns;
        goldmap->map[player_position]  |= current_player;
        goldMine.drawMap();
        sendSignal = true;

      }
      //kill(goldmap->daemonID,SIGUSR1);
    }
    else if( a == 107 ) // 'k' Key for Upward Movement
    {
      if(realGoldFound == true && player_position-NumberOfColumns <= 0)
      {
        sem_wait(GameBoard_Sem);
        goldmap->map[player_position] &= ~current_player;
        sem_post(GameBoard_Sem);
        //kill(goldmap->daemonID,SIGUSR1);
        break;
      }
      if(player_position-NumberOfColumns > 0)
      {
        if(goldmap->map[player_position-NumberOfColumns] != G_WALL)
        {
          goldmap->map[player_position] &= ~current_player;
          player_position = player_position-NumberOfColumns;
          goldmap->map[player_position]  |= current_player;
          goldMine.drawMap();
          sendSignal = true;

        }
        //	kill(goldmap->daemonID,SIGUSR1);
      }

      //	kill(goldmap->daemonID,SIGUSR1);
    }
    else if(a == 81)   /// Q for quit
    {
      running_flag = false;

      sem_wait(GameBoard_Sem);
      goldmap->player--;
      goldmap->map[player_position] &= ~current_player;
      sem_post(GameBoard_Sem);

    }
    else if(a == 77 || a == 109) // key 'M' or 'm' for Message sending
    {
      string rank;
      unsigned int mask=0;
      for(int i = 0; i < 5;i++)
      {
        if(goldmap->player_pid[i] != 0)
        {
          switch (i) {
            case 0:
              mask |= G_PLR0;
              break;
            case 1:
              mask |= G_PLR1;
              break;
            case 2:
              mask |= G_PLR2;
              break;
            case 3:
              mask |= G_PLR3;
              break;
            case 4:
              mask |= G_PLR4;
              break;
          }
        }
      }

      unsigned int destination_player;
      destination_player = mapptr->getPlayer(mask);
      int q_index=0;

      if (destination_player == G_PLR0)
      {
        q_index = 0;
      }
      else if (destination_player == G_PLR1)
      {
        q_index = 1;
      }
      else if(destination_player == G_PLR2)
      {
        q_index = 2;
      }
      else if(destination_player == G_PLR3)
      {
        q_index = 3;
      }
      else if(destination_player == G_PLR4)
      {
        q_index = 4;
      }
      if(current_player == G_PLR0)
      {
        rank="Player 1 says: ";
      }
      else if(current_player == G_PLR1)
      {
        rank="Player 2 says: ";
      }
      else if(current_player == G_PLR2)
      {
        rank="Player 3 says: ";
      }
      else if(current_player == G_PLR3)
      {
        rank="Player 4 says: ";
      }
      else if(current_player == G_PLR4)
      {
        rank="Player 4 says: ";
      }
      msgString = rank+mapptr->getMessage();
      if(current_player != destination_player)
      {
        writeQueue(mqueue_name[q_index],msgString);
      }
    }
    else if(a == 66 || a == 98) // "B" or 'b' key --- broadcast
    {
      string rank;
      if(current_player == G_PLR0)
      {
        rank="Player 1 says: ";
      }
      else if(current_player == G_PLR1)
      {
        rank="Player 2 says: ";
      }
      else if(current_player == G_PLR2)
      {
        rank="Player 3 says: ";
      }
      else if(current_player == G_PLR3)
      {
        rank="Player 4 says: ";
      }
      else if(current_player == G_PLR4)
      {
        rank="Player 4 says: ";
      }
      msgString = rank+mapptr->getMessage();
      for (int i = 0; i < 5;i++)
      {
        if(goldmap->player_pid[i] != 0 && goldmap->player_pid[i]!=getpid())
        {
          writeQueue(mqueue_name[i],msgString);
        }
      }
    }

    if(goldmap->map[player_position] & G_FOOL)
    {
      goldmap->map[player_position] &= ~G_FOOL;
      goldMine.postNotice(" Fools Gold..!!");
    }
    else if(goldmap->map[player_position] & G_GOLD)
    {
      realGoldFound = true;
      goldmap->map[player_position] &= ~G_GOLD;
      goldMine.postNotice(" Real Gold Found..To QUIT go to any edge of the Map!!");
    }

    if(sendSignal == true)
    {
      sendSignal = false;
      refreshScreen(goldmap);
      for(int i = 0; i < 5; i++)
      {
        DEBUG("after movement\n");
        if(goldmap->player_pid[i] != goldmap->daemonID)
        {
          kill(goldmap->daemonID,SIGUSR1);
        }

      }
    }
  }

  sem_wait(GameBoard_Sem);
  if(current_player == G_PLR0)
  {
    if(realGoldFound==true)
    {
      msgString = "Player 1 has won...!!";
      for (int i = 0; i < 5;i++)
      {
        if(goldmap->player_pid[i] != 0 && goldmap->player_pid[i]!=getpid())
        {
          writeQueue(mqueue_name[i],msgString);
        }
      }
    }
    mq_close(readqueue_fd);
    mq_unlink(mqueue_name[0].c_str());
    goldmap->player_pid[0] = 0;
  }
  else if(current_player == G_PLR1)
  {
    if(realGoldFound==true)
    {
      msgString = "Player 2 has won...!!";
      for (int i = 0; i < 5;i++)
      {
        if(goldmap->player_pid[i] != 0 && goldmap->player_pid[i]!=getpid())
        {
          writeQueue(mqueue_name[i],msgString);
        }
      }
    }
    mq_close(readqueue_fd);
    mq_unlink(mqueue_name[1].c_str());
    goldmap->player_pid[1] = 0;
  }
  else if(current_player == G_PLR2)
  {
    if(realGoldFound==true)
    {
      msgString = "Player 3 has won...!!";
      for (int i = 0; i < 5;i++)
      {
        if(goldmap->player_pid[i] != 0 && goldmap->player_pid[i]!=getpid())
        {
          writeQueue(mqueue_name[i],msgString);
        }
      }
    }
    mq_close(readqueue_fd);
    mq_unlink(mqueue_name[2].c_str());
    goldmap->player_pid[2] = 0;
  }
  else if(current_player == G_PLR3)
  {
    if(realGoldFound==true)
    {
      msgString = "Player 4 has won...!!";
      for (int i = 0; i < 5;i++)
      {
        if(goldmap->player_pid[i] != 0 && goldmap->player_pid[i]!=getpid())
        {
          writeQueue(mqueue_name[i],msgString);
        }
      }
    }
    mq_close(readqueue_fd);
    mq_unlink(mqueue_name[3].c_str());
    goldmap->player_pid[3] = 0;
  }
  else if(current_player == G_PLR4)
  {
    if(realGoldFound==true)
    {
      msgString = "Player 5 has won...!!";
      for (int i = 0; i < 5;i++)
      {
        if(goldmap->player_pid[i] != 0 && goldmap->player_pid[i]!=getpid())
        {
          writeQueue(mqueue_name[i],msgString);
        }

      }
    }
    mq_close(readqueue_fd);
    mq_unlink(mqueue_name[4].c_str());
    goldmap->player_pid[4] = 0;
  }

  goldmap->map[player_position] &= ~current_player;

  refreshScreen(goldmap);
  sem_post(GameBoard_Sem);
  kill(goldmap->daemonID, SIGHUP);

  return 0;
}

void create_server_daemon()
{

  if(fork()>0)
  {
    return;
  }
  if(fork()>0)
  {
    exit(0);
  }
  if(setsid()==-1)
  {
    exit(1);
  }

  for(int i = 0; i< sysconf(_SC_OPEN_MAX); ++i)
  {
    if(i != debugFD)//close everything, except write
      close(i);
  }
  open("/dev/null", O_RDWR); //fd 0
  open("/dev/null", O_RDWR); //fd 1
  open("/dev/null", O_RDWR); //fd 2
  umask(0);
  chdir("/");

  //		sleep(3);

  DEBUG("daemon created\n");

  //////////////////////////-----SIGHUP Trapping ----/////////////////////////////

  /*  Sighup*/
  struct sigaction sighup_action;
  //handle with this function
  sighup_action.sa_handler=sighup_handeler;
  //zero out the mask (allow any signal to interrupt)
  sigemptyset(&sighup_action.sa_mask);
  sigaddset(&sighup_action.sa_mask, SIGUSR1);//block SIGUSR1 while handling SIGHUP
  sighup_action.sa_flags=0;
  //tell how to handle SIGHUP
  sigaction(SIGHUP, &sighup_action, NULL);

  /*  sigusr1*/


  struct sigaction sigusr1_action;
  //handle with this function
  sigusr1_action.sa_handler=sigusr1_handeler;
  //zero out the mask (allow any signal to interrupt)
  //		sigemptyset(&sigusr1_action.sa_mask);
  sigusr1_action.sa_flags=0;
  //tell how to handle SIGUSR1
  //server daemon handles SIGUSR1 as sigusr1_handeler
  sigaction(SIGUSR1, &sigusr1_action, NULL);


  /*  sigsr2*/


  struct sigaction sigusr2_action;
  //handle with this function
  sigusr2_action.sa_handler=sigusr2_handeler;
  //zero out the mask (allow any signal to interrupt)
  sigemptyset(&sigusr2_action.sa_mask);
  //	sigaddset(&sigusr2_action.sa_mask, SIGUSR2);//block SIGUSR1 while handling SIGHUP
  sigusr2_action.sa_flags=0;
  //tell how to handle SIGURS2
  sigaction(SIGUSR2, &sigusr2_action, NULL);



  int rows1 =0,cols1 = 0;

  int fdiscriptor = shm_open("/GameBoard_Mem", O_RDWR|O_EXCL , S_IRUSR | S_IWUSR);
  if(fdiscriptor == -1)
  {
    cerr<<"second FD "<<endl;
  }

  // read shared map
  //int map_rows, map_columns ;
  READ(fdiscriptor, &rows1, sizeof(int));
  READ(fdiscriptor, &cols1, sizeof(int));

  goldmap = (GameBoard*)mmap(NULL, rows1*cols1+sizeof(GameBoard), PROT_READ|PROT_WRITE, MAP_SHARED, fdiscriptor, 0);

  if(goldmap == MAP_FAILED )
  {
    DEBUG("MAP Reading for Daemon\n");
  }
  int rows = 0 , cols =0 ;

  rows = goldmap->rows;
  cols = goldmap->cols;


  local_mapcopy = new unsigned char[(rows*cols)];
  goldmap->daemonID=getpid();

  int status; //for error checking

  //change this # between 2000-65k before using
  const char* portno="42424";
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints)); //zero out everything in structure
  hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
  hints.ai_socktype=SOCK_STREAM; // TCP stream sockets
  hints.ai_flags=AI_PASSIVE; //file in the IP of the server for me

  struct addrinfo *servinfo;
  if((status=getaddrinfo(NULL, portno, &hints, &servinfo))==-1)
  {
    //fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }
  sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

  /*avoid "Address already in use" error*/
  int yes=1;
  if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1)
  {
    //perror("setsockopt");
    exit(1);
  }

  //We need to "bind" the socket to the port number so that the kernel
  //can match an incoming packet on a port to the proper process
  if((status=bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
  {
    //perror("bind");
    exit(1);
  }
  //when done, release dynamically allocated memory
  freeaddrinfo(servinfo);

  if(listen(sockfd,1)==-1)
  {
    //perror("listen");
    exit(1);
  }

  //	printf("Blocking, waiting for client to connect\n");

  struct sockaddr_in client_addr;
  socklen_t clientSize=sizeof(client_addr);
  //int new_sockfd;

  do{
    new_sockfd=accept(sockfd, (struct sockaddr*) &client_addr, &clientSize);
  }while(new_sockfd==-1 && errno==EINTR);

  sockfd = new_sockfd;
  DEBUG("accepted\n");
  //read & write to the socket



  for(int x = 0; x < rows*cols ; x++)
  {
    local_mapcopy[x] = goldmap->map[x];
  }


  //WRITE(debugFD, buffer, sizeof(buffer));

  if(goldmap->rows == 26)
  {
    DEBUG("12344\n");
  }
  if(goldmap->cols == 80)
  {
    DEBUG("fdxcghjk\n");
  }

  WRITE(new_sockfd,&goldmap->rows,sizeof(goldmap->rows));
  WRITE(new_sockfd,&goldmap->cols,sizeof(goldmap->cols));

  unsigned char* mptr = local_mapcopy;

  for(int i = 0 ; i < (goldmap->rows*goldmap->cols);i++)
  {
    unsigned char bit =local_mapcopy[i];
    WRITE(new_sockfd,&bit,sizeof(bit));
  }

  unsigned char SockPlayer = G_SOCKPLR;

  for(int i=0; i<5; ++i)
  {
    if(goldmap->player_pid[i]!=0)
    {
      switch(i)
      {
        case 0:
          SockPlayer|=G_PLR0;
          break;
        case 1:
          SockPlayer|=G_PLR1;
          break;
        case 2:
          SockPlayer|=G_PLR2;
          break;
        case 3:
          SockPlayer|=G_PLR3;
          break;
        case 4:
          SockPlayer|=G_PLR4;
          break;
      }
    }
  }

  WRITE(new_sockfd, &SockPlayer,sizeof(SockPlayer));
  //	WRITE(debugFD, &SockPlayer, sizeof(SockPlayer));  //debug
  continuous_listen();



  //add when exiting
  //close(new_sockfd);

  ////////////////////////////////SOCKET END//////////////////////////////////////
}


void client_deamon(string client_ip)
{
  pipe(pipefd);
  ////////////////////////////////CLIENT DAEMON START////////////////////////////////////
  if(fork()>0)
  {
    close(pipefd[1]); //close write, parent only needs read
    int val;
    READ(pipefd[0], &val, sizeof(val));
    if(val==1)
      WRITE(1, "Success!\n", sizeof("Success!\n"));
    else
    {
      WRITE(1, "Failure!\n", sizeof("Failure!\n"));
    }
    //	wait(NULL);//reap zombie
    return ;
  }
  if(fork()>0)
    exit(0);
  if(setsid()==-1)
    exit(1);
  for(int i = 0; i< sysconf(_SC_OPEN_MAX); ++i)
  {
    if(i!=pipefd[1] && i !=debugFD)//close everything, except write
    {
      close(i);
    }
  }


  open("/dev/null", O_RDWR); //fd 0
  open("/dev/null", O_RDWR); //fd 1
  open("/dev/null", O_RDWR); //fd 2
  umask(0);
  chdir("/");

  DEBUG("client deamon running\n");
  ////////////////////////////////DAEMON END////////////////////////////////////
  //--------------------signal Trapping-----------------//


  /*  Sighup*/
  struct sigaction sighup_action;
  //handle with this function
  sighup_action.sa_handler=sighup_handeler;
  //zero out the mask (allow any signal to interrupt)
  sigemptyset(&sighup_action.sa_mask);
  sigaddset(&sighup_action.sa_mask, SIGUSR1);//block SIGUSR1 while handling SIGHUP
  sighup_action.sa_flags=0;
  //tell how to handle SIGHUP
  sigaction(SIGHUP, &sighup_action, NULL);

  /*  sigusr1*/


  struct sigaction sigusr1_action;
  //handle with this function
  sigusr1_action.sa_handler=sigusr1_handeler;
  //zero out the mask (allow any signal to interrupt)
  sigemptyset(&sigusr1_action.sa_mask);
  //	sigaddset(&sigusr1_action.sa_mask, SIGUSR1);//block SIGUSR1 while handling SIGHUP
  sigusr1_action.sa_flags=0;
  //tell how to handle SIGUSR1
  //client daemon handles SIGUSR1 as sigusr1_handeler
  sigaction(SIGUSR1, &sigusr1_action, NULL);


  /*  sigsr2*/

  struct sigaction sigusr2_action;
  //handle with this function
  sigusr2_action.sa_handler=sigusr2_handeler;
  //zero out the mask (allow any signal to interrupt)
  sigemptyset(&sigusr2_action.sa_mask);
  //sigaddset(&sigusr2_action.sa_mask, SIGUSR2);//block SIGUSR1 while handling SIGHUP
  sigusr2_action.sa_flags=0;
  //tell how to handle SIGURS2
  sigaction(SIGUSR2, &sigusr2_action, NULL);


  //---------------------------signal trapping---------------//

  ////////////////////////////////CLIENT SOCKET///////////////////////////////////

  //int sockfd; //file descriptor for the socket
  //int status; //for error checking

  //change this # between 2000-65k before using
  const char* portno="42424";

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints)); //zero out everything in structure
  hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
  hints.ai_socktype=SOCK_STREAM; // TCP stream sockets

  struct addrinfo *servinfo;
  //instead of "localhost", it could by any domain name
  if((status=getaddrinfo(client_ip.c_str(), portno, &hints, &servinfo))==-1)
  {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }
  sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

  if((status=connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
  {
    perror("connect");
    exit(1);
  }

  new_sockfd = sockfd;
  //release the information allocated by getaddrinfo()
  freeaddrinfo(servinfo);
  int rows1,cols1;
  // reading rows and colomns

  READ(new_sockfd,&rows1,sizeof(rows1));
  READ(new_sockfd,&cols1,sizeof(cols1));

  // reading map from server

  local_mapcopy = new unsigned char[(rows1 * cols1)];

  //unsigned char read_from_server;


  int client_mapSize = rows1 * cols1;

  for(int i = 0; i < client_mapSize ;i++)
  {
    READ(new_sockfd, &local_mapcopy[i],sizeof(local_mapcopy[i]));
    //localClient[i] = read_from_server;
  }

  //SEM OPEN

  GameBoard_Sem = sem_open("/GameBoard_Sem",O_RDWR|O_CREAT,S_IROTH|S_IWOTH|S_IRGRP|S_IWGRP|S_IRUSR|S_IWUSR,1);
  client_shared_mem=shm_open("/GameBoard_Mem",O_RDWR|O_CREAT,S_IWUSR|S_IRUSR);
  ftruncate(client_shared_mem ,client_mapSize + sizeof(GameBoard));
  goldmap = (GameBoard*) mmap(0,client_mapSize + sizeof(GameBoard),PROT_WRITE,MAP_SHARED,client_shared_mem,0);

  goldmap->rows = rows1;
  goldmap->cols = cols1;
  goldmap->daemonID = getpid();

  for(int i = 0; i < client_mapSize ;i++)
  {
    goldmap->map[i] = local_mapcopy[i];
  }

  unsigned char client_byte;

  READ(new_sockfd, &client_byte,sizeof(client_byte));

  unsigned char player_bit[5]={G_PLR0, G_PLR1, G_PLR2, G_PLR3, G_PLR4};
  for(int i = 0; i < 5 ; i ++)
  {
    if(client_byte & player_bit[i] )
    {
      goldmap->player_pid[i] = goldmap->daemonID; //need to check
    }
  }


  int val=1;
  WRITE(pipefd[1], &val, sizeof(val));

  continuous_listen();
}
////////////////////////////CLIENT SOCKET END///////////////////////////////////

void sighup_handeler(int z)
{
  unsigned char SockPlayer1;
  SockPlayer1 = G_SOCKPLR;

  for(int i=0; i<5; ++i)
  {
    if(goldmap->player_pid[i]!=0)
    {
      switch(i)
      {
        case 0:
          SockPlayer1|=G_PLR0;
          break;
        case 1:
          SockPlayer1|=G_PLR1;
          break;
        case 2:
          SockPlayer1|=G_PLR2;
          break;
        case 3:
          SockPlayer1|=G_PLR3;
          break;
        case 4:
          SockPlayer1|=G_PLR4;
          break;
      }
    }
  }
  WRITE(sockfd, &SockPlayer1,sizeof(SockPlayer1));


  if(SockPlayer1 == G_SOCKPLR)
  {
    sem_close(GameBoard_Sem);
    sem_unlink("/GameBoard_Sem");
    shm_unlink("/GameBoard_Mem");
    exit(1);
  }
}


void sigusr2_handeler(int z)
{

}

void sigusr1_handeler(int z)
{
  DEBUG("inside sihuser1 handler \n");
  vector< pair<short,unsigned char> > pvec;
  //  unsigned char* shared_memory_map = gm->map;
  for(short i=0; i< (goldmap->rows*goldmap->cols); ++i)
  {
    if(goldmap->map[i] != local_mapcopy[i])
    {
      pair<short,unsigned char> aPair;
      aPair.first=i;
      aPair.second=goldmap->map[i];
      pvec.push_back(aPair);
      local_mapcopy[i] = goldmap->map[i];  /// ckeck local copy name for client
    }
  }

  unsigned char zero = 0;

  short pvec_size1 = pvec.size();
  //if (pvec.size() > 0 )
  //{
  WRITE(new_sockfd,&zero, sizeof(zero));
  WRITE(new_sockfd,&pvec_size1,sizeof(pvec_size1));
  //		WRITE(debugFD,&pvec_size,sizeof(pvec_size));
  for(short i = 0; i < pvec_size1; ++i)
  {
    WRITE(new_sockfd,&pvec[i].first,sizeof(pvec[i].first));
    WRITE(new_sockfd,&pvec[i].second,sizeof(pvec[i].second));
  }
  /// socket write : Socket map
  //}
  DEBUG("at the end sihuser1 handler \n");
}


void handle_interrupt(int)
{
  mapptr->drawMap();
}

void continuous_listen()
{
  while(1)
  {
    unsigned char sb;
    READ(new_sockfd,&sb,sizeof(sb));

    if(sb & G_SOCKPLR)
    {
      unsigned char player_bit[5]={G_PLR0, G_PLR1, G_PLR2,G_PLR3, G_PLR4};
      for(int i=0; i<5; ++i) //loop through the player bits
      {
        if((sb & player_bit[i]) && (goldmap->player_pid[i] == 0))
        {
          goldmap->player_pid[i] = goldmap->daemonID;
          //WRITE(debugFD,&goldmap->player_pid[i], sizeof(goldmap->player_pid[i]));
        }
        else if(!(sb & player_bit[i]) && (goldmap->player_pid[i] == 0))
        {
          goldmap->player_pid[i] = 0;
        }
      }
      //	refreshScreen(goldmap);
    }
    if(sb == G_SOCKPLR)
    {
      //kill(goldmap->daemonID,SIGHUP);
      //unlink code sem and shm
      sem_close(GameBoard_Sem);
      sem_unlink("/GameBoard_Sem");
      shm_unlink("/GameBoard_Mem");
    }
    else if (sb == 0)
    {
      //	option		listen_var=9000;
      short pvec_size;

      //sig user 1 bheja
      READ(new_sockfd,&pvec_size,sizeof(pvec_size));

      DEBUG("pvec_size=%d\n",pvec_size);

      for(short i = 0; i < pvec_size;i++)
      {
        DEBUG("inside for\n");

        short  pvec_first_read;
        unsigned char pvec_second_read;
        READ(new_sockfd,&pvec_first_read,sizeof(pvec_first_read));
        READ(new_sockfd,&pvec_second_read,sizeof(pvec_second_read));
        local_mapcopy[pvec_first_read] = pvec_second_read;
        goldmap->map[pvec_first_read] = pvec_second_read;
        //	refreshScreen(goldmap);
        for(int i=0;i<5;i++)
        {

          if(goldmap->player_pid[i]!= 0 && goldmap->player_pid[i]!=getpid())
          {
            DEBUG("insidde for refresh\n");
            kill(goldmap->player_pid[i], SIGUSR1);
          }

        }

      }

    }
    // else if( condition for msg)
    // {
    // // need to write
    // }


  }
}





/** Function to insert Fool's and Real Gold into the Map*/

int insertGold(unsigned char* map,int No_Of_Gold,int NumberOfRows,int NumberOfColumn)
{
  unsigned char* tempMap = map;
  srand(time(NULL));
  int position;
  bool insert_flag = true;
  int counter = No_Of_Gold-1;

  while(insert_flag)
  {
    position = rand() % (NumberOfRows*NumberOfColumn);

    if(tempMap[position] == 0)
    {
      if(counter != 0)
      {
        tempMap[position] = G_FOOL;
        counter--;
      }
      else
      {
        tempMap[position] = G_GOLD;
        insert_flag = false;
      }
    }
  }
  return 0;
}

/** Function to insert players into the Map*/
int insertPlayer(unsigned char* map,unsigned char player, int NumberOfRows, int NumberOfColumn)
{
  unsigned char* tempMap2 = map;
  srand(time(NULL));
  int position=0;
  while(1)
  {
    position = rand() % (NumberOfRows*NumberOfColumn);
    if(tempMap2[position] == 0)
    {
      tempMap2[position] = player;
      return position;
    }
  }
}

void read_message(int)  //// msg que read
{
  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(readqueue_fd, &mq_notification_event);

  //read a message
  int err;
  char msg[121];
  memset(msg, 0, 121);//set all characters to '\0'
  while((err=mq_receive(readqueue_fd, msg, 120, NULL))!=-1)
  {
    mapptr->postNotice(msg);
    memset(msg, 0, 121);//set all characters to '\0'
  }
  //we exit while-loop when mq_receive returns -1
  //if errno==EAGAIN that is normal: there is no message waiting
  if(errno!=EAGAIN)
  {
    perror("mq_receive");
    exit(1);
  }
}

void clean_up(int)  /// msg queue cleanup
{
  mq_close(readqueue_fd);
  sig=true;
}


void readQueue(string mqueue_name)
{
  //I have added this signal-handling
  //code so that if you type ctrl-c to
  //abort the long, slow loop at the
  //end of main, then your message queue
  //will be properly cleaned up.
  if((readqueue_fd=mq_open(mqueue_name.c_str(), O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,
          S_IRUSR|S_IWUSR, &mq_attributes))==-1)
  {
    perror("mq_open 123");
    exit(1);
  }
  //set up message queue to receive signal whenever message comes in
  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(readqueue_fd, &mq_notification_event);

}

void writeQueue(string mqueue_name,string msgString)
{
  mqd_t writequeue_fd; //message queue file descriptor
  if((writequeue_fd = mq_open(mqueue_name.c_str(), O_WRONLY|O_NONBLOCK))==-1)
  {
    perror("mq_open :");
    exit(1);
  }
  char message_text[121];
  memset(message_text, 0, 121);
  strncpy(message_text, msgString.c_str(), 120);

  if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
  {
    perror("mq_send");
    exit(1);
  }
  mq_close(writequeue_fd);
}

void refreshScreen(GameBoard* goldmap)
{
  for(int i =0 ; i < 5; i++)
  {
    if((goldmap->player_pid[i] != 0) && (goldmap->player_pid[i] != getpid()))
    {
      kill(goldmap->player_pid[i], SIGUSR1);
    }
  }
}
