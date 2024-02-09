// TODO: steps 46 to 54 (except 49). I'm holding back on those steps for
// now because I'm using a completely different set of keybindings than
// the original tutorial.

/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define VOID_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

enum Mode{
  NORMAL,
};

typedef struct erow{
  int size;
  char *chars;
} erow;

struct ed_config{
  int cx, cy;
  int scrows, sccols;
  int numrows;
  erow *row;
  enum Mode mode;
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
/*** row operations ***/

void voided_append_row(char *s, size_t len){
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.numrows++;
}

/*** file i/o ***/

void voided_open(char *filename){
  FILE *fp = fopen(filename, "r");
  if(!fp) die("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while((linelen = getline(&line, &linecap, fp)) != -1){
    while(linelen > 0 && (line[linelen - 1] == '\n' ||
                          line[linelen - 1] == '\r'))
      linelen--;
    voided_append_row(line, linelen);
  }
  free(line);
  fclose(fp);
}

/*** append buffer ***/

struct abuf{
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

void ab_append(struct abuf *ab, const char *s, int len){
  char *new = realloc(ab->b, ab->len + len);
  if(new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void ab_free(struct abuf *ab){
  free(ab->b);
}

/*** output ***/

void voided_draw_rows(struct abuf *ab){
  int y;
  for(y = 0; y < E.scrows; y++){
    if(y >= E.numrows){
      if(E.numrows == 0 && y == E.scrows / 3){
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
                                  "Void editor -- version %s", VOID_VERSION);
        if(welcomelen > E.sccols) welcomelen = E.sccols;
        int padding = (E.sccols - welcomelen) / 2;
        if(padding){
          ab_append(ab, "~", 1);
          padding--;
        }
        while(padding--) ab_append(ab, " ", 1);
        ab_append(ab, welcome, welcomelen);
      } else {
        ab_append(ab, "~", 1);
      }
    } else {
      int len = E.row[y].size;
      if(len > E.sccols) len = E.sccols;
      ab_append(ab, E.row[y].chars, len);
    }
    ab_append(ab, "\x1b[K", 3);
    if(y < E.scrows - 1){
      ab_append(ab, "\r\n", 2);
    }
  }
}

void voided_refresh_screen(){
  struct abuf ab = ABUF_INIT;

  ab_append(&ab, "\x1b[?25l", 6);
  ab_append(&ab, "\x1b[2J", 4);
  ab_append(&ab, "\x1b[H", 3);

  voided_draw_rows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy+ 1, E.cx + 1);
  ab_append(&ab, buf, strlen(buf));

  ab_append(&ab, "\x1b[?25h", 6);
  write(STDOUT_FILENO, ab.b, ab.len);

  ab_free(&ab);
}

/*** input ***/

void voided_move_cursor(char key){
  switch(E.mode){
    case NORMAL:
      switch(key){
        case 'h':
          if(E.cx != 0){
            E.cx--;
          }
          break;
        case 'j':
          if(E.cy != E.scrows - 1){
            E.cy++;
          }
          break;
        case 'k':
          if(E.cy != 0){
            E.cy--;
          }
          break;
        case 'l':
          if(E.cx != E.sccols - 1){
            E.cx++;
          }
          break;
      }
      break;
  }
}

void voided_process_keypress(){
  char c = voided_read_key();

  switch(E.mode){
    case NORMAL:
      switch(c){
        case CTRL_KEY('q'):
          write(STDOUT_FILENO, "\x1b[2J", 4);
          write(STDOUT_FILENO, "\x1b[H", 3);
          exit(0);
          break;
        case 'h':
        case 'j':
        case 'k':
        case 'l':
          voided_move_cursor(c);
          break;
      }
  }
}

/*** init ***/

void voided_init(){
  E.cx = 0;
  E.cy = 0;
  E.numrows = 0;
  E.row = NULL;
  E.mode = NORMAL;
  if(get_window_size(&E.scrows, &E.sccols) == -1) die("get_window_size");
}

int main(int argc, char **argv){
  enable_raw_mode();
  voided_init();

  if(argc >= 2){
    voided_open(argv[1]);
  }

  while(1){
    voided_refresh_screen();
    voided_process_keypress();
  }
  return 0;
}
