/*** includes ***/

#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_VERSION "0.0.1"

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  PAGE_UP,
  PAGE_DOWN,
  HOME_KEY,
  END_KEY,
  BACKSPACE,
  DEL_KEY,
  PREV_KEY,
  NEWLINE_KEY,
  ADD_CHAR_KEY
};

/*** data ***/

typedef struct erow {
    int size;
    char* chars;
} erow;

struct editorConfig {
    int screenrows;
    int screencols;
    struct termios orig_termios;
    int cx, cy;
    int numrows;
    erow *row;
    int rowoff;
    int coloff;
    char *filename;
};
struct editorConfig E;

/*** terminal ***/

void die(const char *s){
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void disableRawMode(){
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1){
        die("tcsetattr");
    }
}
void enableRawMode(){
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);
    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

void editorInsertChar(int c){
    if (E.numrows == 0) {
        // Create first row if none exists
        E.row = malloc(sizeof(erow));
        E.row[0].size = 0;
        E.row[0].chars = malloc(1);
        E.row[0].chars[0] = '\0';
        E.numrows = 1;
        E.cy = 0;
        E.cx = 0;
    }
    
    // Ensure cursor is within bounds
    if (E.cy >= E.numrows) E.cy = E.numrows - 1;
    if (E.cy < 0) E.cy = 0;
    
    int insertPos = E.cx;
    erow *row = &E.row[E.cy];
    
    // Ensure cursor position is within row bounds
    if (insertPos > row->size) insertPos = row->size;
    if (insertPos < 0) insertPos = 0;
    
    int oldSize = row->size;
    row->chars = realloc(row->chars, oldSize + 2);
    memmove(&row->chars[insertPos + 1], &row->chars[insertPos], oldSize - insertPos + 1);
    row->chars[insertPos] = (char)c;
    row->size = oldSize + 1;
    row->chars[row->size] = '\0';
    E.cx = insertPos + 1;
}

void editorDeleteChar(void) {
    if (E.numrows == 0) return;

    // if we're at the beginning of the file
    if (E.cy == 0 && E.cx == 0) return;

    erow *row = &E.row[E.cy];

    // case 1: delete character within line
    if (E.cx > 0) {
        memmove(&row->chars[E.cx - 1], &row->chars[E.cx], row->size - E.cx + 1);
        row = &E.row[E.cy];
        row->size--;
        E.cx--;
        return;
    }

    // case 2: at beginning of line -> merge with previous
    if (E.cx == 0) {
        int prev_size = E.row[E.cy - 1].size;
        E.row[E.cy - 1].chars = realloc(E.row[E.cy - 1].chars, prev_size + row->size + 1);
        memcpy(&E.row[E.cy - 1].chars[prev_size], row->chars, row->size + 1);
        E.row[E.cy - 1].size = prev_size + row->size;

        // free current row
        free(row->chars);

        // shift rows up
        memmove(&E.row[E.cy], &E.row[E.cy + 1], sizeof(erow) * (E.numrows - E.cy - 1));
        E.numrows--;

        E.cy--;
        E.cx = prev_size;  // move cursor to end of previous line
    }
}


void editorInsertNewline(void) {
    if (E.numrows == 0) {
        // Create first row if none exists
        E.row = malloc(sizeof(erow));
        E.row[0].size = 0;
        E.row[0].chars = malloc(1);
        E.row[0].chars[0] = '\0';
        E.numrows = 1;
        E.cy = 0;
        E.cx = 0;
        return;
    }
    
    // Ensure cursor is within bounds
    if (E.cy < 0) E.cy = 0;
    if (E.cy >= E.numrows) E.cy = E.numrows - 1;

    erow *row = &E.row[E.cy];
    int split = E.cx;
    
    // Ensure split position is within bounds
    if (split > row->size) split = row->size;
    if (split < 0) split = 0;

    // Allocate space for new line
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

    // Shift existing rows below current line down by one
    memmove(&E.row[E.cy + 1], &E.row[E.cy],
            sizeof(erow) * (E.numrows - E.cy));

    // reaquire new row pointer
    row = &E.row[E.cy];

    // Create the new line below current one
    E.row[E.cy + 1].size = row->size - split;
    E.row[E.cy + 1].chars = malloc(E.row[E.cy + 1].size + 1);
    memcpy(E.row[E.cy + 1].chars, &row->chars[split], E.row[E.cy + 1].size);
    E.row[E.cy + 1].chars[E.row[E.cy + 1].size] = '\0';

    // Truncate the current line at cursor position
    row->size = split;
    row->chars = realloc(row->chars, split + 1);
    row->chars[split] = '\0';

    E.numrows++;
    E.cy++;
    E.cx = 0;
}


void editorMoveCursor(int key) {
    switch (key) {
      case ARROW_LEFT:
          if (E.cx != 0) {
              E.cx--;
          } else if (E.coloff > 0) {
              E.coloff--;
          }
          break;
      case ARROW_RIGHT:
          if (E.numrows > 0 && E.cy < E.numrows && E.cx < E.row[E.cy].size) {
              E.cx++;
          }
          break;
      case ARROW_UP:
        if (E.cy != 0) {
            E.cy--;
        } else if (E.rowoff > 0) {
            E.rowoff--;
        }
        break;
      case ARROW_DOWN:
        if (E.cy < E.numrows - 1) {
            E.cy++;
        }
        break;
    }
    
    // Keep cursor in view
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.cx < E.coloff) {
        E.coloff = E.cx;
    }
    if (E.cx >= E.coloff + E.screencols) {
        E.coloff = E.cx - E.screencols + 1;
    }
  }

int editorReadKey(){
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1)!= 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1)!= 1) return '\x1b';

        if (seq[0] == '['){
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch(seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch(seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return '\x1b';
    } else {
        if (isprint(c)) {
            editorInsertChar(c);
            return ADD_CHAR_KEY;
        } else if (c == '\r') {
            return NEWLINE_KEY;
        } else if (c == 127 || c == '\b') { // Backspace (mac = 127)
            return BACKSPACE;
        }
        return c;
    }

}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
    printf("\r\n");
    char c;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0]!= '\x1b' || buf[1]!= '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** file i/o  ***/

void editorFree() {
    for (int i = 0; i < E.numrows; i++) {
        free(E.row[i].chars);
    }
    free(E.row);
}

void editorSave() {
  if (E.numrows == 0) return;
  
  FILE *fp = fopen(E.filename, "w");
  if (!fp) die("fopen");
  
  for (int i = 0; i < E.numrows; i++) {
    fwrite(E.row[i].chars, E.row[i].size, 1, fp);
    if (i < E.numrows - 1) {
      fwrite("\n", 1, 1, fp);
    }
  }
  
  fclose(fp);
}

void editorOpen(char *filename) {
  E.filename = filename;
  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");

  E.numrows = 0;
  E.row = NULL;
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                           line[linelen - 1] == '\r'))
        linelen--;

    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

    E.row[E.numrows].size = linelen;
    E.row[E.numrows].chars = malloc(linelen + 1);
    memcpy(E.row[E.numrows].chars, line, linelen);
    E.row[E.numrows].chars[linelen] = '\0';
    E.numrows++;
  }
  free(line);
  fclose(fp);
  
  // If no lines were read, create an empty first line
  if (E.numrows == 0) {
    E.numrows = 1;
    E.row = malloc(sizeof(erow));
    E.row[0].size = 0;
    E.row[0].chars = malloc(1);
    E.row[0].chars[0] = '\0';
  }
}

/*** append buffer ***/
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT { NULL, 0 }

void abAppend(struct abuf *ab, const char *s, int len){
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab){
    free(ab->b);
}

/*** input ***/

void editorProcessKey(){
    int c = editorReadKey();

    switch (c) {
        case CTRL_KEY('s'):
            editorSave();
            break;
        case CTRL_KEY('q'):
            editorSave();
            editorFree();
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;

        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            E.cx = E.screencols - 1;
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times = E.screenrows;
                while (times--)
                editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
        case NEWLINE_KEY:
            editorInsertNewline();
            break;
        case BACKSPACE:
            editorDeleteChar();
            break;
        case ADD_CHAR_KEY:
            break;
    }

    }

/*** output ***/

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows){
        if (y == E.screenrows / 3) {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION);
            if (welcomelen > E.screencols) welcomelen = E.screencols;
            int padding = (E.screencols - welcomelen) / 2;
            if (padding) {
                abAppend(ab, "~", 1);
                padding--;
            }
            while (padding--) abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcomelen);
        } else{
            abAppend(ab, "~", 1);
        }
    } else {
        int len = E.row[filerow].size - E.coloff;
        if (len < 0) len = 0;
        if (len > E.screencols) len = E.screencols;
        for (int i = 0; i < len; i++) {
            abAppend(ab, &E.row[filerow].chars[E.coloff + i], 1);
        }
    }

    abAppend(ab, "\x1b[K", 3);
    if (y < E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorScroll() {
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }

    if (E.cx < E.coloff) {
        E.coloff = E.cx;
    }
    if (E.cx >= E.coloff + E.screencols) {
        E.coloff = E.cx - E.screencols + 1;
    }
}


void editorRefreshScreen() {
    editorScroll();
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    // Ensure cursor is within bounds before positioning
    if (E.numrows == 0) {
        E.cy = 0;
        E.cx = 0;
    } else {
        if (E.cy >= E.numrows) E.cy = E.numrows - 1;
        if (E.cy < 0) E.cy = 0;
        if (E.cx > E.row[E.cy].size) E.cx = E.row[E.cy].size;
        if (E.cx < 0) E.cx = 0;
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.cx - E.coloff) + 1);
    abAppend(&ab, buf, strlen(buf));


    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** init ***/

void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 1;
    E.row = malloc(sizeof(erow));
    E.row[0].size = 0;
    E.row[0].chars = malloc(1);
    E.row[0].chars[0] = '\0';
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");

}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  } else {
    // HARDCODED FILENAME
    E.filename = "temp.txt";
  }

  while (1) {
      editorRefreshScreen();
      editorProcessKey();
  }
  return 0;
}
