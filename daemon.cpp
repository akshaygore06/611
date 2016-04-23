#include<unistd.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/stat.h>

int main()
{
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
  sleep(1000);
}
