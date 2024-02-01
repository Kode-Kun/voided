/*** includes ***/

#include <asm-generic/ioctls.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct ed_config{
  int scrows;
  int sccols;
  struct termios orig_term;
};

struct ed_config E;

/*** terminal ***/

void die(const char *s){
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

void disable_raw_mode(){
  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_term) == -1)
  die("tcsetattr");
}

void enable_raw_mode(){
  if(tcgetattr(STDIN_FILENO, &E.orig_term) == -1) die("tcgetattr");
  atexit(disable_raw_mode);

  struct termios raw = E.orig_term;
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

int get_cursor_position(int *rows, int *cols){
  char buf[32];
  unsigned int i = 0;

  if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  while(i < sizeof(buf) - 1){
    if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if(buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';

  if(buf[0] != '\x1b' || buf[1] != '[') return -1;
  if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  return 0;
}

int get_window_size(int *rows, int *cols){
  struct winsize ws;

  if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
    if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return get_cursor_position(rows, cols);
  }else{
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** output ***/

void voided_draw_rows(){
  int y;
  for(y = 0; y < E.scrows; y++){
    write(STDOUT_FILENO, "~", 1);

    if(y < E.scrows -1){
      write(STDOUT_FILENO, "\r\n", 2);
    }
  }
}

void voided_refresh_screen(){
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  voided_draw_rows();

  write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** input ***/

void voided_process_keypress(){
  char c = voided_read_key();

  switch(c){
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
  }
}

/*** init ***/

void voided_init(){
  if(get_window_size(&E.sccols, &E.scrows) == -1) die("get_window_size");
}

int main(){
  enable_raw_mode();
  voided_init();

  while(1){
    voided_refresh_screen();
    voided_process_keypress();
  }
  return 0;
}
