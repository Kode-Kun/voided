/*** includes ***/

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct termios orig_term;

/*** terminal ***/

void die(const char *s){
  perror(s);
  exit(1);
}

void disable_raw_mode(){
  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term) == -1)
  die("tcsetattr");

}

void enable_raw_mode(){
  if(tcgetattr(STDIN_FILENO, &orig_term) == -1) die("tcgetattr");
  atexit(disable_raw_mode);

  struct termios raw = orig_term;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char voided_read_key(){
  int nread;
  char c;
  while((nread = read(STDIN_FILENO, &c, 1)) != 1){
    if(nread == -1 && errno != EAGAIN) die("read");
  }
  return c;
}

/*** input ***/

void voided_process_keypress(){
  char c = voided_read_key();

  switch(c){
    case CTRL_KEY('q'):
      exit(0);
      break;
  }
}

/*** init ***/

int main(){
  enable_raw_mode();
  while(1){
    voided_process_keypress();
  }
  return 0;
}
