// TODO: steps 46 to 54 (except 49). I'm holding back on those steps for
// now because I'm using a completely different set of keybindings than
// the original tutorial.

/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/*** defines ***/

#define VOID_VERSION "0.0.1"
#define VOID_TAB_STOP 8

// keyboard-related macros
#define CTRL_KEY(k) ((k) & 0x1f)

#define MV_RIGHT 'l'
#define MV_LEFT 'h'
#define MV_DOWN 'j'
#define MV_UP 'k'

#define ESC 27

/*** data ***/

enum EdKey{
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN,
};

enum Mode{
  NORMAL,
  INSERT,
  //VISUAL,
  //COMMAND,
};

typedef struct erow{
  int size;
  int rsize;
  char *chars;             // string with row's contents
  char *render;            // string that gets rendered
} erow;

struct ed_config{
  int cx, cy;              // cursor x and y
  int rx;                  // cursor x position in render string
  int rowoff, coloff;      // row offset and column offset
  int scrows, sccols;      // screen rows and screen columns (receives value from get_window_size())
  int numrows;             // total number of rows
  erow *row;               // holds all rows in the currently opened file
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;   // time elapsed since status msg was first drawn
  enum Mode mode;
  struct termios orig_term;
};

struct ed_config E;        // global editor config

/*** terminal ***/

// kill voided and print errors
void die(const char *s){
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

// revert terminal to its initial state
void disable_raw_mode(){
  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_term) == -1)
  die("tcsetattr");
}

// put terminal in raw mode; for full access to input and output
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

// reads key from stdin and returns the key
char voided_read_key(){
  int nread;
  char c;
  while((nread = read(STDIN_FILENO, &c, 1)) != 1){
    if(nread == -1 && errno != EAGAIN) die("read");
  }
  return c;
}

// asks the terminal for status information and puts it in rows and cols 
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

// converts cx to rx, dealing with tabs
int row_cx_to_rx(erow *row, int cx){
  int rx = 0;
  int j;
  for(j = 0; j < cx; j++){
    if(row->chars[j] == '\t')
      rx += (VOID_TAB_STOP - 1) - (rx % VOID_TAB_STOP);
    rx++;
  }
  return rx;
}

// updates the render variable in row (for rendering tabs)
void voided_update_row(erow *row){
  int tabs = 0;
  int j;
  for(j=0; j< row->size; j++){
    if(row->chars[j] == '\t') tabs++;
  }
  free(row->render);
  row->render = malloc(row->size + tabs*(VOID_TAB_STOP - 1) + 1);

  int idx = 0;
  for(j = 0; j < row->size; j++){
    if(row->chars[j] == '\t'){
      row->render[idx++] = ' ';
      while(idx % VOID_TAB_STOP != 0) row->render[idx++] = ' ';
    } else{
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;
}

// appends s of size len to last row
void voided_append_row(char *s, size_t len){
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  voided_update_row(&E.row[at]);

  E.numrows++;
}

void voided_row_insert_char(erow *row, int at, int c){
  if(at < 0 || at > row->size) at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  voided_update_row(row);
}

/*** editor operations ***/

void voided_insert_char(int c){
  if(E.cy == E.numrows){
    voided_append_row("", 0);
  }
  voided_row_insert_char(&E.row[E.cy], E.cx, c);
  E.cx++;
}

// sets status message and resets time if t isn't 0
void voided_set_status_msg(const char *fmt, const char t, ...){
  va_list ap;
  va_start(ap, t);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  if(t != 0){
    E.statusmsg_time = time(NULL);
  } else{
    E.statusmsg_time = 0;
  }
}

/*** file i/o ***/

// converts all rows into one big heap-allocated buffer.
// ***DO NOT FORGET TO FREE THE RETURNED BUF!***
char *voided_rows_to_string(int *buflen){
  int totlen = 0;
  int j;
  for(j = 0; j < E.numrows; j++)
    totlen += E.row[j].size + 1;
  *buflen = totlen;

  char *buf = malloc(totlen);
  char *p = buf;
  for(j = 0; j < E.numrows; j++){
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';
    p++;
  }

  return buf;
}

// opens file and appends each line to a row 
void voided_open(char *filename){
  free(E.filename);
  E.filename = strdup(filename);

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

char voided_save(){
  if(E.filename == NULL){
    voided_set_status_msg("failed to save file", 1);
    return 1;      // dont do anything if its a new file
  }
  int len;
  char *buf = voided_rows_to_string(&len);

  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if(fd != -1){
    if(ftruncate(fd, len) != -1){
      if(write(fd, buf, len) == len){
	close(fd);
	free(buf);
	voided_set_status_msg("wrote %d bytes to '%s'", 1, len, E.filename);
	return 0;
      }
    }
    close(fd);
  }
  free(buf);
  voided_set_status_msg("can't save! I/O error: %s", 1, strerror(errno));
  return 0;
}

/*** append buffer ***/

// append buffer struct used to write escape sequences and text to the terminal
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

// operations to be done whenever cy changes or when rx goes out of bounds
void voided_scroll(){
  E.rx = 0;
  if(E.cy < E.numrows){
    E.rx = row_cx_to_rx(&E.row[E.cy], E.cx);
  }

  if(E.cy < E.rowoff){
    E.rowoff = E.cy;
  }
  if(E.cy >= E.rowoff + E.scrows){
    E.rowoff = E.cy - E.scrows + 1;
  }
  if(E.rx < E.coloff){
    E.coloff = E.rx;
  }
  if(E.rx >= E.coloff + E.sccols){
    E.coloff = E.rx - E.sccols + 1;
  }
}

// iterates through each row and renders it accordingly
// also deals with welcome message  
void voided_draw_rows(struct abuf *ab){
  int y;
  for(y = 0; y < E.scrows; y++){
    int filerow = y + E.rowoff;
    if(filerow >= E.numrows){
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
      int len = E.row[filerow].rsize - E.coloff;
      if(len < 0) len = 0;
      if(len > E.sccols) len = E.sccols;
      ab_append(ab, &E.row[filerow].render[E.coloff], len);
    }
    ab_append(ab, "\x1b[K", 3);
    ab_append(ab, "\r\n", 2);
  }
}

void voided_draw_status_bar(struct abuf *ab){
  ab_append(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  char *filename;
  int fn_size;
  if (E.filename != NULL){
    filename = strdup(E.filename);
    fn_size = strlen(E.filename);
  } else{
    filename = NULL;
    fn_size = 0;
  }
  if (fn_size > 20){
    int j = 3;
    int i;
    for(i = 0; i < j; i++){
      filename[(20 - (j-i))] = '.';
    }
  }
  int len = snprintf(status, sizeof(status), "%.20s - %d lines",
                     filename ? filename : "[No Name]", E.numrows);
  int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.numrows);
  if(len > E.sccols) len = E.sccols;
  ab_append(ab, status, len);
  while(len < E.sccols){
    if(E.sccols - len == rlen){
      ab_append(ab, rstatus, rlen);
      break;
    } else{
      ab_append(ab, " ", 1);
      len++;
    }
  }
  ab_append(ab, "\x1b[m", 3);
  ab_append(ab, "\r\n", 2);
  free(filename);
}

void voided_draw_msg_bar(struct abuf *ab){
  ab_append(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if(msglen > E.sccols) msglen = E.sccols;
  if((msglen && time(NULL) - E.statusmsg_time < 5) || E.statusmsg_time == 0)
    ab_append(ab, E.statusmsg, msglen);
}

// called every frame. most render-related functions are called here
void voided_refresh_screen(){
  voided_scroll();

  struct abuf ab = ABUF_INIT;

  ab_append(&ab, "\x1b[?25l", 6);
  ab_append(&ab, "\x1b[2J", 4);
  ab_append(&ab, "\x1b[H", 3);

  voided_draw_rows(&ab);
  voided_draw_status_bar(&ab);
  voided_draw_msg_bar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
                                            (E.rx - E.coloff) + 1);
  ab_append(&ab, buf, strlen(buf));

  ab_append(&ab, "\x1b[?25h", 6);
  write(STDOUT_FILENO, ab.b, ab.len);

  ab_free(&ab);
}


/*** input ***/

// called whenever a cursor movement key is pressed 
void voided_move_cursor(char key){
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

  switch(key){
    case MV_LEFT:
      if(E.cx != 0){
        E.cx--;
      } else if(E.cy > 0){
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case MV_DOWN:
      if(E.cy < E.numrows){
        E.cy++;
      }
      break;
    case MV_UP:
      if(E.cy != 0){
        E.cy--;
      }
      break;
    case MV_RIGHT:
      if(row && E.cx < row->size){
        E.cx++;
      } else if(row && E.cx == row->size){
        E.cy++;
        E.cx = 0;
      }
      break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if(E.cx > rowlen){
    E.cx = rowlen;
  }
}

// handles normal mode key presses
void voided_process_normal(int c){
  switch(c){
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
    case CTRL_KEY('s'):
      voided_save();
      break;
    case CTRL_KEY(MV_UP):
    case CTRL_KEY(MV_DOWN):
      {
        if(c == CTRL_KEY(MV_UP)){
          E.cy = E.rowoff;
        } else if(c == CTRL_KEY(MV_DOWN)){
          E.cy = E.rowoff + E.scrows - 1;
          if(E.cy > E.numrows) E.cy = E.numrows;
        }
        int times = E.scrows;
        while(times--){
          voided_move_cursor(c == CTRL_KEY(MV_UP) ? MV_UP : MV_DOWN);
        }
      }
      break;
    case '$':
      if(E.cy < E.numrows)
        E.cx = E.row[E.cy].size;
      break;
    case MV_DOWN:
    case MV_UP:
    case MV_RIGHT:
    case MV_LEFT:
      voided_move_cursor(c);
      break;
    case 'i':
      E.mode = INSERT;
      voided_set_status_msg("--INSERT--", 0);
      break;
  }
}

// handles insert mode key presses
void voided_process_insert(int c){
  switch(c){
    case ESC:
      E.mode = NORMAL;
      voided_set_status_msg("", 0);
      break;
    case '\r':
      // TODO
      break;
    case BACKSPACE:
    case DEL_KEY:
      // TODO
      break;
    case CTRL_KEY(MV_DOWN):
      voided_move_cursor(MV_DOWN);
      break;
    case CTRL_KEY(MV_UP):
      voided_move_cursor(MV_UP);
      break;
    case CTRL_KEY(MV_RIGHT):
      voided_move_cursor(MV_RIGHT);
      break;
    case CTRL_KEY(MV_LEFT):
      voided_move_cursor(MV_LEFT);
      break;
    default:
      voided_insert_char(c);
      break;
  }
}

// called every frame, cursor-related functions are called here
void voided_process_keypress(){
  char c = voided_read_key();

  switch(E.mode){
    case NORMAL:
      voided_process_normal(c);
      return;
    case INSERT:
      voided_process_insert(c);
      return;
  }
}

/*** init ***/

void voided_init(){
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;
  E.mode = NORMAL;

  if(get_window_size(&E.scrows, &E.sccols) == -1) die("get_window_size");
  E.scrows -= 2;
}

int main(int argc, char **argv){
  enable_raw_mode();
  voided_init();

  if(argc >= 2){
    voided_open(argv[1]);
  }

  voided_set_status_msg("HELP: Ctrl-S = save | Ctrl-Q = quit", 1);

  while(1){
    voided_refresh_screen();
    voided_process_keypress();
  }
  return 0;
}
