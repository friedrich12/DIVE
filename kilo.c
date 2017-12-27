#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <termios.h>
#include <ctype.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdio.h>

#define DIVE_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

struct editorConfig {
    int cx, cy;
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;


struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void 
disMode(){
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1){
        err("tcsetattr");
    }
}

void 
abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL) return;

  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void 
abFree(struct abuf *ab) {
  free(ab->b);
}

void 
err(const char *s){
    write(STDIN_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void
setMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1){
      err("tcgetattr");
  }
  atexit(disMode);

  struct termios raw = E.orig_termios;
  // use bitwiae=AND to make
  // every fouth bit 0
  // read bit by bit
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1){
      err("tcsetattr");
  }
}


int 
getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }

  buf[i] = '\0';
  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
  return 0;
}


int 
getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

char 
editorReadKey(){
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if (nread == -1 && errno != EAGAIN) err("read");
    }
    return c;
}

void 
editorProcessKeypress(){
    char c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDIN_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case 'w':
            break;
        case 'a':
            break;
        case 's':
            break;
    }
}

void 
editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    if (y == E.screenrows / 3) {
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome),
        "DIVE EDITOR -- VERSION %s", DIVE_VERSION);
      if (welcomelen > E.screencols) welcomelen = E.screencols;
      int padding = (E.screencols - welcomelen) / 2;
      if(padding){
          abAppend(ab, "~", 1);
          padding--;
      }
      while(padding--) abAppend(ab, " ", 1);
      abAppend(ab, welcome, welcomelen);
    } else {
      abAppend(ab, "~", 1);
    }
    abAppend(ab, "\x1b[K", 3);
    if (y < E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}


void 
editorRefreshScreen() {
  //Terminal control codes
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[H", 3);
  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

void
initEditor(){
    E.cy = 0;
    E.cx = 0;
    if(getWindowSize(&E.screenrows, &E.screencols) == -1){
        err("getWindowSize");
    }
}


int 
main(){
    setMode();
    initEditor();

    char c;
    while (1){
       editorRefreshScreen();
       editorProcessKeypress();
    }
    return 0;
}