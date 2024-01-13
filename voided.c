#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>

struct termios orig_term;

void disable_raw_mode(){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
}

void enable_raw_mode(){
    tcgetattr(STDIN_FILENO, &orig_term);
    atexit(disable_raw_mode);

    struct termios raw = orig_term;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(){
    enable_raw_mode();
    char c;
    while(read(STDIN_FILENO, &c, 1) == 1 && c != 'q'){
        if(iscntrl(c)){
            printf("%d\r\n", c);
        } else{
            printf("%d ('%c')\r\n", c, c);
        }
    }
    return 0;
}
