#include "editor.h"
#include "terminal.h"
#include "fileio.h"
#include "autocomplete.h"

/*** editor functions ***/

void editorAllocateNewRow(void){
    E.row = malloc(sizeof(erow));
    E.row[0].size = 0;
    E.row[0].chars = malloc(1);
    E.row[0].chars[0] = '\0';
    E.numrows = 1;
    E.cy = 0;
    E.cx = 0;
    return;
}

void editorInsertChar(int c){
    if (E.numrows == 0) {
        // Create first row if none exists
        editorAllocateNewRow();
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
    row = &E.row[E.cy];
    row->chars[insertPos] = (char)c;
    row->size = oldSize + 1;
    row->chars[row->size] = '\0';
    E.cx = insertPos + 1;
    E.dirty = 1;
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
        E.dirty = 1;
        return;
    }

    // case 2: at beginning of line -> merge with previous
    if (E.cx == 0) {
        int prev_size = E.row[E.cy - 1].size;
        E.row[E.cy - 1].chars = realloc(E.row[E.cy - 1].chars, prev_size + row->size + 1);
        memcpy(&E.row[E.cy - 1].chars[prev_size], row->chars, row->size + 1);
        E.row[E.cy - 1].size = prev_size + row->size;
        E.dirty = 1;

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
        editorAllocateNewRow();
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
    E.dirty = 1;
}

void editorMoveCursor(int key) {
    switch (key) {
      case ARROW_LEFT:
          if (E.cx > 0) {
              E.cx--;
          } else if (E.cy > 0) {
              E.cy--;
              E.cx = E.row[E.cy].size;
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

void editorProcessKey(void){
    int c = editorReadKey();

    switch (c) {
        case CTRL_KEY('s'):
            editorSave();
            break;
        case CTRL_KEY('q'):
        case CTRL_KEY('c'):
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
            if (autocompleteIsActive()){
                autocompleteSelectPrev(); break;
            }
        case ARROW_DOWN:
        case ARROW_LEFT:
            if (autocompleteIsActive()){
                autocompleteSelectNext(); break;
            }
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
        case NEWLINE_KEY:
            if (autocompleteIsActive()) {autocompleteAcceptSuggestion(); autocompleteHideSuggestions(); break;}
            editorInsertNewline();
            break;
        case BACKSPACE:
            editorDeleteChar();
            break;
        case TAB_KEY:
            if (autocompleteIsActive()){
                autocompleteAcceptSuggestion();
            } else {
                erow* row = &E.row[E.cy];
                int start = E.cx;
                while (start > 0 && isalnum((unsigned char) row->chars[start - 1])) start--;

                int wordLen = E.cx - start;
                char word[MAX_WORD_LENGTH];
                if (wordLen > 0 && wordLen < MAX_WORD_LENGTH){
                    memcpy(word, &row->chars[start], wordLen);
                    word[wordLen] = '\0';
                    autocompleteUpdateSuggestions(word, E.cy, E.cx);
                    autocompleteShowSuggestions();
                }
            }
            break;
        default:
            editorInsertChar(c);


            // after inserting, recompute current word and update suggestions
            erow* row = &E.row[E.cy];
            int start = E.cx;
            while (start > 0 && isalnum((unsigned char) row->chars[start - 1])) start--;

            int wordLen = E.cx - start;
            char word[MAX_WORD_LENGTH];
            if (wordLen >= 2 && wordLen < MAX_WORD_LENGTH){
                memcpy(word, &row->chars[start], wordLen);
                word[wordLen] = '\0';
                autocompleteUpdateSuggestions(word, E.cy, E.cx);
                autocompleteShowSuggestions();
            }
            break;
    }
}

void editorScroll(void) {
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

void editorDrawStatusBar(struct abuf *ab) {
    abAppend(ab, "\x1b[7m", 4); // invert colors
    char status[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                       E.filename, E.numrows, E.dirty ? "(modified)" : "");
    if (len > E.screencols) len = E.screencols;
    abAppend(ab, status, len);
    while (len < E.screencols) {
        abAppend(ab, " ", 1);
        len++;
    }
    abAppend(ab, "\x1b[m", 3); // reset
}

void editorRefreshScreen(void) {
    editorScroll();
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    autocompleteDrawSuggestions(&ab);

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

void initEditor(void) {
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 1;
    E.row = malloc(sizeof(erow));
    E.row[0].size = 0;
    E.row[0].chars = malloc(1);
    E.row[0].chars[0] = '\0';
    E.dirty = 0;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
  E.screenrows -= 1;
  autocompleteInit();
}
