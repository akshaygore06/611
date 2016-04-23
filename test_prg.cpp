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
#include<stdlib.h> //for exit
#include "fancyRW.h"


using namespace std;

/** GameBoard Structure for the Goldchase */

struct GameBoard {
	int rows; //4 bytes
	int cols; //4 bytes
	pid_t player_pid[5];
	unsigned char player ;
	unsigned char map[0];
	int daemonID;

};

/** Global Variables **/
Map *mapptr;
mqd_t readqueue_fd; //message queue file descriptor
struct mq_attr mq_attributes;
string mqueue_name[5];
bool sig=false;
int debugFD;
GameBoard* goldmap;
GameBoard* gm;
int fd;
unsigned char* local_mapcopy;
sem_t* GameBoard_Sem;

int NumberOfRows = -1;
int NumberOfColumns = -1;

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
void client(string);

int main(int argc, char* argv[])
{
	/** Variable Declaration */
	ifstream inputFile;
	string Line,CompleteMapString= "";
	int No_Of_Gold,mapSize;
	bool firstline = true;
	bool collen_flag = true;

	char* theMine;
	const char* ptr;
	unsigned char* mp;
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

/*
	argv checking
*/

//string client_ip = "localhost" ;
	if(argc == 2)
	{
	//	string client_ip = argv[1] ;
	//	client(client_ip);
	client ("192.168.98.195");
	}




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
		read(fdiscriptor, &map_rows, sizeof(int));
		read(fdiscriptor, &map_columns, sizeof(int));

		goldmap = (GameBoard*)mmap(NULL, map_rows*map_columns+sizeof(GameBoard), PROT_READ|PROT_WRITE, MAP_SHARED, fdiscriptor, 0);
		if(goldmap == MAP_FAILED )
		{
			cerr<<"MAP Reading for Else Part "<<endl;
		}

		NumberOfColumns = goldmap->cols;
		NumberOfRows = goldmap->rows;

		if(goldmap->player_pid[0] == 0)
		{
			current_player = G_PLR0;
			goldmap->player_pid[0] = getpid();
			readQueue(mqueue_name[0]);
		}
		else if (goldmap->player_pid[1] == 0)
		{
			current_player = G_PLR1;
			goldmap->player_pid[1] = getpid();
			readQueue(mqueue_name[1]);
		}
		else if (goldmap->player_pid[2] == 0)
		{
			current_player = G_PLR2;

			goldmap->player_pid[2] =getpid();
		readQueue(mqueue_name[2]);
		}
		else if (goldmap->player_pid[3] == 0)
		{
			current_player = G_PLR3;
			goldmap->player_pid[3] = getpid();
			readQueue(mqueue_name[3]);

		}
		else if (goldmap->player_pid[4] == 0)
		{
			current_player = G_PLR4;
			goldmap->player_pid[4] = getpid();
			readQueue(mqueue_name[4]);
		}
		else
		{
			cout << "No more players allowed" <<endl;
			return 0;
		}

		player_position = insertPlayer(goldmap->map,current_player,NumberOfRows,NumberOfColumns);
	}

	sem_wait(GameBoard_Sem);
	goldmap->player |= current_player;
	sem_post(GameBoard_Sem);

	Map goldMine(goldmap->map,goldmap->rows,goldmap->cols);


	mapptr = &goldMine;

	refreshScreen(goldmap);
	goldmap->daemonID = 0;
//	cout<<"deamon ig before :"<< goldmap->daemonID << endl;
	if(goldmap->daemonID == 0 )
	{
			//std::cerr<<"inside deomon call"<<std::endl;
			create_server_daemon();

			//cout <<"2345678"<<endl;
			//std::cerr << "end of deamon creation" << std::endl;
	}
	else
	{
			//send sighup signal
	}


	/** Code of Movement*/


	int currentColumn,currentRow;
	int a=0;
	bool running_flag =  true,realGoldFound=false, sendSignal = false;

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
				break;
			}
			if(goldmap->map[player_position-1] != G_WALL )
			{
				goldmap->map[player_position] &= ~current_player;
				player_position = player_position -1;
				goldmap->map[player_position]  |= current_player;
				sendSignal = true;
			}
		}
		else if( a == 108 ) /// 'l' Key for Right Movement
		{
			if(realGoldFound == true && currentColumn == goldmap->cols -1)
			{
				sem_wait(GameBoard_Sem);
				goldmap->map[player_position] &= ~current_player;
				sem_post(GameBoard_Sem);
				break;
			}
			if(goldmap->map[player_position+1] != G_WALL )//| currentColumn != 0 )
			{
				goldmap->map[player_position] &= ~current_player;
				player_position = player_position+1;
				goldmap->map[player_position]  |= current_player;
				sendSignal = true;
			}
		}
		else if( a == 106 ) // 'j' Key for Down Movement
		{
			if(realGoldFound == true && currentRow >= NumberOfRows-1 )
			{
				sem_wait(GameBoard_Sem);
				goldmap->map[player_position] &= ~current_player;
				sem_post(GameBoard_Sem);
				break;
			}
			if(goldmap->map[player_position+NumberOfColumns] != G_WALL  && currentRow < NumberOfRows-1 )
			{
				goldmap->map[player_position] &= ~current_player;
				player_position = player_position+NumberOfColumns;
				goldmap->map[player_position]  |= current_player;
				sendSignal = true;
			}
		}
		else if( a == 107 ) // 'k' Key for Upward Movement
		{
			if(realGoldFound == true && player_position-NumberOfColumns <= 0)
			{
				sem_wait(GameBoard_Sem);
				goldmap->map[player_position] &= ~current_player;
				sem_post(GameBoard_Sem);
				break;
			}
			if(player_position-NumberOfColumns > 0)
			{
				if(goldmap->map[player_position-NumberOfColumns] != G_WALL)
				{

					goldmap->map[player_position] &= ~current_player;
					player_position = player_position-NumberOfColumns;
					goldmap->map[player_position]  |= current_player;
					sendSignal = true;
				}
			}
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

	if(goldmap->player_pid[0]== 0 && goldmap->player_pid[1]== 0 && goldmap->player_pid[2]== 0 && goldmap->player_pid[3]== 0 && goldmap->player_pid[4]== 0)
	{
		// sem_close(GameBoard_Sem);
		// sem_unlink("/GameBoard_Sem");
		// shm_unlink("/GameBoard_Mem");
		kill(goldmap->daemonID, SIGHUP);
	}
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

	  for(int i; i< sysconf(_SC_OPEN_MAX); ++i)
		{
			close(i);
		}
	  open("/dev/null", O_RDWR); //fd 0
	  open("/dev/null", O_RDWR); //fd 1
	  open("/dev/null", O_RDWR); //fd 2
	  umask(0);
	  chdir("/");

		//sleep(3);
		debugFD = open("/home/akshay/private_git_611/611/mypipe",O_WRONLY);

		write(debugFD, "daemon created\n", sizeof("daemon created\n"));

		int shm_fd = shm_open("/GameBoard_Mem",O_EXCL|O_RDWR , S_IRUSR | S_IWUSR);

		if(shm_fd == -1)
		{
		//	cerr<<"deamon shm_Fd -- "<<endl;
			write(debugFD, "ERROR : deamon shm_Fd \n", sizeof("ERROR : deamon shm_Fd \n"));
		}


		//goldmap->daemonID = getpid();
		int rows,cols;
		read(shm_fd, &rows, sizeof(int));
		read(shm_fd, &cols, sizeof(int));



		gm = (GameBoard*)mmap(NULL, rows*cols+sizeof(GameBoard), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);

		if(gm== MAP_FAILED )
		{
			write(debugFD, "MAP Reading for Daemon\n", sizeof("MAP Reading for Daemon\n"));
			// cerr<<"MAP Reading for Daemon "<<endl;
		}



		gm->daemonID = getpid();

		local_mapcopy = new unsigned char[rows*cols+sizeof(char)];

		//unsigned char local_mapcopy[rows*cols];

		for(int x = 0; x < rows*cols ; x++)
		{
			local_mapcopy[x] = gm->map[x];
		}

/*   ---- print local memory spit out

		for(int i =0;i < rows*cols ; i++)
		{

			if(local_mapcopy[i] == 0)
			{
				write(debugFD," ",sizeof(" "));
			}
			else if(local_mapcopy[i] == G_WALL)
			{
					write(debugFD,"*",sizeof("*"));
			}
		}


*/
//
// 		for(int i = 0; i < (r * c); ++i)
// {
//   write(fd, &localCopy[i],sizeof(localCopy[i]));
// }


//////////////////////////-----SIGHUP Trapping ----/////////////////////////////

/*  Sighup*/
		struct sigaction sighup_action;
		//handle with this function
		sighup_action.sa_handler=sighup_handeler;
		//zero out the mask (allow any signal to interrupt)
		sigemptyset(&sighup_action.sa_mask);
		sighup_action.sa_flags=0;
		//tell how to handle SIGHUP
		sigaction(SIGHUP, &sighup_action, NULL);

/*  sigusr1*/


		struct sigaction sigusr1_action;
		//handle with this function
		sigusr1_action.sa_handler=sigusr1_handeler;
		//zero out the mask (allow any signal to interrupt)
		sigemptyset(&sigusr1_action.sa_mask);
		sigusr1_action.sa_flags=0;
		//tell how to handle SIGURS1
		sigaction(SIGUSR1, &sigusr1_action, NULL);


/*  sigsr2*/


		struct sigaction sigusr2_action;
		//handle with this function
		sigusr2_action.sa_handler=sigusr2_handeler;
		//zero out the mask (allow any signal to interrupt)
		sigemptyset(&sigusr2_action.sa_mask);
		sigusr2_action.sa_flags=0;
		//tell how to handle SIGURS2
		sigaction(SIGUSR2, &sigusr2_action, NULL);




//----------------------------------------------------------------------------//
	// while(1)
	// {
	// 	write(debugFD, "Daemon is running!\n", sizeof("Daemon is running!\n"));
	// 	sleep(2);
	// }

/////////////////////////////////////SOCKET/////////////////////////////////////

int sockfd; //file descriptor for the socket
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
	fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
	exit(1);
}
sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

/*avoid "Address already in use" error*/
int yes=1;
if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1)
{
	perror("setsockopt");
	exit(1);
}

//We need to "bind" the socket to the port number so that the kernel
//can match an incoming packet on a port to the proper process
if((status=bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
{
	perror("bind");
	exit(1);
}
//when done, release dynamically allocated memory
freeaddrinfo(servinfo);

if(listen(sockfd,1)==-1)
{
	perror("listen");
	exit(1);
}

		write(debugFD, "Blocking, waiting for client to connect\n", sizeof("Blocking, waiting for client to connect\n"));
  //printf("Blocking, waiting for client to connect\n");


  struct sockaddr_in client_addr;
  socklen_t clientSize=sizeof(client_addr);
  int new_sockfd;
	//cerr << "cerr Accepting\n";
  if((new_sockfd=accept(sockfd, (struct sockaddr*) &client_addr, &clientSize))==-1)
  {
    perror("accept");
    exit(1);
  }
write(debugFD,"Accepted\n",sizeof("Accepted\n")); /// writting to pipe
  //read & write to the socket
  char buffer[100];
 memset(buffer,0,100);
  int n;
  if((n=read(new_sockfd, buffer, 99))==-1)
  {
    perror("read is failing");
    printf("n=%d\n", n);
  }

	sleep(10);
			  // ///writing to the socket
				// if(write(new_sockfd, &rows, sizeof(int)) == -1)
				// 	{
				// 		perror("writing error");
				// 	}
				// if(write(new_sockfd, &cols, sizeof(int)) == -1)
				// 	{
				// 		perror("writing error");
				// 	}
				//
				// if(write(new_sockfd, local_mapcopy, rows*cols) ==-1)
				// 	{
				// 		perror("writing error");
				// 	}
				// 	while(1)
				// 	{
				// 	}

  // char arr[11];
  // memset(arr, 0, 11);
  // READ(new_sockfd, arr, 3);
  // cout << "first read=" << arr << endl;
  // memset(arr, 0, 11);
  // READ(new_sockfd, arr, 4);
  // cout << "second read=" << arr << endl;
  // memset(arr, 0, 11);
  // READ(new_sockfd, arr, 3);
  // cout << "third read=" << arr << endl;

//  printf("The client said: %s\n", buffer);

//std::cerr << "/* The client said: \n"<< buffer << std::endl;
  WRITE(debugFD,buffer,sizeof(buffer)); //  debuging pipe
	const char* message="These are the times that try men's souls.\n";
  //cerr << "\nwriting";
	write(new_sockfd, message, strlen(message));
  close(new_sockfd);



////////////////////////////////SOCKET END//////////////////////////////////////
}

void client(string client_ip)
{

////////////////////////////////DAEMON START////////////////////////////////////
	if(fork()>0)
  {
    //I'm the parent, leave the function
    return;
  }

  if(fork()>0)
    exit(0);
  if(setsid()==-1)
    exit(1);
  for(int i; i< sysconf(_SC_OPEN_MAX); ++i)
    close(i);
  open("/dev/null", O_RDWR); //fd 0
  open("/dev/null", O_RDWR); //fd 1
  open("/dev/null", O_RDWR); //fd 2
  umask(0);
  chdir("/");

	debugFD = open("/home/akshay/private_git_611/611/mypipe",O_WRONLY);
	write(debugFD,"client deamon running\n", sizeof("client deamon running\n"));
////////////////////////////////DAEMON END////////////////////////////////////

////////////////////////////////CLIENT SOCKET///////////////////////////////////


	int sockfd; //file descriptor for the socket
  int status; //for error checking

  //change this # between 2000-65k before using
  const char* portno="42424";

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints)); //zero out everything in structure
  hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
  hints.ai_socktype=SOCK_STREAM; // TCP stream sockets

  struct addrinfo *servinfo;
  //instead of "localhost", it could by any domain name
  //if((status=getaddrinfo("localhost", portno, &hints, &servinfo))==-1)
	if((status=getaddrinfo(client_ip.c_str(), portno, &hints, &servinfo))==-1)
  {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }
  sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

	if( sockfd == -1)
	{
		perror(" no sockfd");
	}

  if((status=connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
  {
    perror("connect deamon");
    exit(1);
  }
  //release the information allocated by getaddrinfo()
  freeaddrinfo(servinfo);

	int rows = 0 ,cols=0;
	int mapSize = 0;
	int client_shared_mem;
	GameBoard* client_shared_map;

	// READ(sockfd,&rows,sizeof(int));
  // READ(sockfd,&cols,sizeof(int));
	//
	// mapSize = rows* cols;
	//
  // client_shared_mem =shm_open("/Client_SharedMemAG",O_RDWR|O_CREAT,S_IRUSR|S_IWUSR);
  // ftruncate(client_shared_mem,mapSize + sizeof(GameBoard));
	// client_shared_map = (GameBoard*)mmap(NULL, mapSize+sizeof(GameBoard), PROT_READ|PROT_WRITE, MAP_SHARED, client_shared_mem , 0);
	//
	//
	//
	//

	write(debugFD,"before reading \n", sizeof("before reading \n"));

//const char* message="One small step for (a) man, one large  leap for Mankind";
 const char* message = "Todd Gibson\n";
  int n ;
  if((n=sockfd, message, strlen(message))==-1)
  {
    perror("write");
    exit(1);
  }

  write(sockfd, "Client --> server working..!! \n", sizeof("Client --> server working..!! \n"));


  //printf("client wrote %d characters\n", n);
  char buffer[100];

	write(debugFD,buffer, sizeof(buffer));
//	perror("3456789");
  memset(buffer, 0, 100);
  read(sockfd, buffer, 99);
  //READ(sockfd, buffer, 99);
  printf("%s\n", buffer);
  close(sockfd);

////////////////////////////CLIENT SOCKET END///////////////////////////////////

}







void sighup_handeler(int z)
{
	sem_close(GameBoard_Sem);
	sem_unlink("/GameBoard_Sem");
	shm_unlink("/GameBoard_Mem");
}


void sigusr2_handeler(int z)
{

}
void sigusr1_handeler(int z)
{
	vector< pair<short,unsigned char> > pvec;
  unsigned char* shared_memory_map = goldmap->map;
	for(short i=0; i<goldmap->rows*goldmap->cols; ++i)
  {
    if(shared_memory_map[i]!=local_mapcopy[i])
    {
      pair<short,unsigned char> aPair;
      aPair.first=i;
      aPair.second=shared_memory_map[i];
      pvec.push_back(aPair);
      local_mapcopy[i]=shared_memory_map[i];
    }
  }



	// if (pvec.size() > 0 )
	// {
	// 	/// socket write : Socket map
	// }

}


void handle_interrupt(int)
{
	mapptr->drawMap();
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
		if(goldmap->player_pid[i] != 0 )
		{
				kill(goldmap->player_pid[i], SIGUSR1);
		}
	}
}
