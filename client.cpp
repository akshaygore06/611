//Socket client
#include <sys/types.h>
#include<sys/socket.h>
#include<unistd.h> //for read/write
#include<netdb.h>
#include<string.h> //for memset
#include<stdio.h> //for fprintf, stderr, etc.
#include<stdlib.h> //for exit
#include "fancyRW.h"

int main()
{
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
  if((status=getaddrinfo("localhost", portno, &hints, &servinfo))==-1)
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
  //release the information allocated by getaddrinfo()
  freeaddrinfo(servinfo);

 const char* message="One small step for (a) man, one large  leap for Mankind";
//  const char* message = "Todd Gibson yudi jagani";
  int n ;
  if((n=WRITE(sockfd, message, strlen(message)))==-1)
  {
    perror("write");
    exit(1);
  }

  //WRITE(sockfd, "1234567890", 10);


  printf("client wrote %d characters\n", n);
  char buffer[100];
  memset(buffer, 0, 100);
  read(sockfd, buffer, 99);
  //READ(sockfd, buffer, 99);
  printf("%s\n", buffer);
  close(sockfd);
  return 0;
}
