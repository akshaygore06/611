#include<unistd.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/stat.h>
int debugFD;
void create_daemon()
{
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

  //All of the stuff for "Server start up" goes here
  //
  //For a first step, instead of #5:
  //"Listen on socket for a client to connect"
  //Open up a named pipe for writing. You will have a file descriptor
  //to that named pipe. Later, that exact same variable will be the
  //file descriptor for the socket.
  //
  //Now, write the signal handlers. This is most of the code, right here.
}


int main()
{
  create_daemon();
  debugFD = open("mypipe",O_RDWR);
   while(1)
 {
   write(debugFD, "Daemon is running!\n", sizeof("Daemon is running!\n"));
   sleep(2);
 }
  return 0;
}
