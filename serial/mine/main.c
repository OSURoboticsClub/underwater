#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BAUDRATE B9600
#define BUF_S 255

int main(int argc, char **argv)
{
  int in;
  int out;
  int res;
  char buf[BUF_S];
  if (argc < 3)
  {
    printf("need a read terminal and a write terminal\n");
    exit(0);
  }
  in = open(argv[1], O_RDWR | O_NOCTTY);
  out = open(argv[2], O_RDWR | O_NOCTTY);

  res = read(in, buf, BUF_S);
  //buf[res+1]='\0';
  fwrite(buf, res, 1, stdout);
  res = write(out, buf, BUF_S);
  //fflush(stdout);
  //printf("%s", buf);

  return 0;
}
