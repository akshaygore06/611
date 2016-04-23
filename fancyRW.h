// /*
//  * write template functions that are guaranteed to read and write the
//  * number of bytes desired
//  */
 #include <iostream>
 #include <unistd.h>
 #include <errno.h>

#ifndef fancyRW_h
#define fancyRW_h
int x = 0;
int y=0;
template<typename T>
int READ(int fd, T* obj_ptr, int count)
{
  char* addr=(char*)obj_ptr;
  int y = 0;
 label:
  y = read(fd, addr+y, count-y);

  if(errno == EINTR)
  {
    goto label;
  }
  else if(y == -1)
  {
     return y;
  }
  return y;
}

template<typename T>
int WRITE(int fd, T* obj_ptr, int count)
{
  char* addr=(char*)obj_ptr;

  x = write(fd, addr, count);

  while(errno == EINTR )
  {
    x = write(fd, addr+x, count-x);
  }

  return x;
}
#endif
