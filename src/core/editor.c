#include "editor.h"
#include "common.h"
#include "terminal.h"
#include "fileio.h"
#include "autocomplete.h"
#include "syntax.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/*** static helpers ***/
static int int_to_str(int v, char *out) {
    char tmp[16];
    int n = 0;
    if (v == 0) { out[0] = '0'; return 1; }
    while (v > 0 && n < (int)sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    return n;
}

/*** editor functions ***/



void editorSearchStart(void) {
    E.search_active = 1;
    E.search_len = 0;
    E.search_query[0] = '\0';
    E.search_saved_cx = E.cx;
    E.search_saved_cy = E.cy;
    E.search_match_row = -1;
    E.search_match_col = -1;
    E.search_match_len = 0;
}

void editorSearchCancel(void) {
    E.search_active = 0;
    E.search_len = 0;
    E.search_query[0] = '\0';
    E.cx = E.search_saved_cx;
    E.cy = E.search_saved_cy;
    E.search_match_row = -1;
    E.search_match_col = -1;
    E.search_match_len = 0;
}

void editorSearchCommit(void) {
    E.search_active = 0;
}

void editorSearchUpdate(void) {
    if (E.search_len == 0) return;
    for (int i = 0; i < E.numrows; i++){
        erow *row = &E.row[i];
        char *match = strstr(row->chars, E.search_query);
        if (match) {
            int match_col = (int)(match - row->chars);
            E.cy = i;
            E.cx = match_col + E.search_len;
            E.search_match_row = i;
            E.search_match_col = match_col;
            E.search_match_len = E.search_len;
            return;
        }
    }
    E.search_match_row = -1;
    E.search_match_col = -1;
    E.search_match_len = 0;
}

void editorGotoStart(void) {
    E.goto_active = 1;
    E.goto_len = 0;
    E.goto_buf[0] = '\0';
    E.goto_saved_cx = E.cx;
    E.goto_saved_cy = E.cy;
}

void editorGotoCancel(void) {
    E.goto_active = 0;
    E.goto_len = 0;
    E.goto_buf[0] = '\0';
    E.cx = E.goto_saved_cx;
    E.cy = E.goto_saved_cy;
}

void editorGotoCommit(void) {
    E.goto_active = 0;
}

void editorGotoUpdate(void) {
    if (E.goto_len == 0) return;
    int line = atoi(E.goto_buf) - 1;
    if (line < 0) line = 0;
    if (line >= E.numrows) line = E.numrows - 1;
    E.cy = line;
    if (E.cx > E.row[E.cy].size) E.cx = E.row[E.cy].size;
}

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
    char* new_chars = realloc(row->chars, oldSize + 2);
    if (!new_chars){
        return;
    }
    row->chars = new_chars;
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
    }

    // case 2: at beginning of line -> merge with previous
    else if (E.cx == 0) {
        int prev_size = E.row[E.cy - 1].size;
        char* new_chars = realloc(E.row[E.cy - 1].chars, prev_size + row->size + 1);
        if (!new_chars){
            return;
        }
        E.row[E.cy - 1].chars = new_chars;
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
    erow* new_row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    if (!new_row){
        return;
    }
    E.row = new_row;

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
    char* new_chars = realloc(row->chars, split + 1);
    if (!new_chars){
        return;
    }
    row->chars = new_chars;
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
        default:
            break;
        }

    if (E.numrows > 0 && E.cy >= 0 && E.cy < E.numrows) {
        if (E.cx > E.row[E.cy].size) {
            E.cx = E.row[E.cy].size;
        }
        if (E.cx < 0) {
            E.cx = 0;
        }
    } else {
        E.cx = 0;
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
    int buffer_changed = 0;
    int did_reparse = 0;
    if (E.cy >= E.numrows) E.cy = E.numrows - 1;
    if (E.cy < 0) E.cy = 0;
    if (E.cx > E.row[E.cy].size) E.cx = E.row[E.cy].size;
    if (E.cx < 0) E.cx = 0;

    if (E.search_active) {
        if (c == '\x1b') {
            if (autocompleteIsActive()){
                autocompleteHideSuggestions();
                return;
            }
            editorSearchCancel();
            return;
        } else if (c == NEWLINE_KEY || c == ENTER) {
            editorSearchCommit();
            return;
        } else if (c == BACKSPACE) {
            if (E.search_len > 0){
                E.search_len--;
                E.search_query[E.search_len] = '\0';
                editorSearchUpdate();
            }
            return;
        } else if (isprint((unsigned char)c)) {
            if (E.search_len < (int) sizeof(E.search_query) - 1) {
                E.search_query[E.search_len++] = (char)c;
                E.search_query[E.search_len] = '\0';
                editorSearchUpdate();
            }
            return;
        }
    }

    if (E.goto_active) {
        if (c == '\x1b') {
            editorGotoCancel();
            return;
        } else if (c == NEWLINE_KEY || c == ENTER){
            editorGotoCommit();
            return;
        } else if (c == BACKSPACE) {
            if (E.goto_len > 0) {
                E.goto_len--;
                E.goto_buf[E.goto_len] = '\0';
                editorGotoUpdate();
            }
            return;
        } else if (isdigit((unsigned char) c)) {
            if (E.goto_len < (int) sizeof(E.goto_buf) - 1) {
                E.goto_buf[E.goto_len++] = (char) c;
                E.goto_buf[E.goto_len] = '\0';
                editorGotoUpdate();
            }
            return;
        }
    }

    switch (c) {
        case CTRL_KEY('s'):
            editorSave();
            break;
        case CTRL_KEY('l'):
            editorSearchStart();
            break;
        case CTRL_KEY('j'):
            // TODO: ADD jump to line!
            editorGotoStart();
            break;
        case CTRL_KEY('d'):
            E.debug_tree = !E.debug_tree;
            break;
        case CTRL_KEY('q'):
        case CTRL_KEY('c'):
            editorSave();
            editorFree();
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            syntaxFree();
            exit(0);
            break;
        case CTRL_KEY('a'):
            // Move to start of line
            E.cx = 0;
            break;
        case CTRL_KEY('e'):
            if (E.numrows > 0 && E.cy >= 0 && E.cy < E.numrows) {
                E.cx = E.row[E.cy].size;
            } else {
                E.cx = 0;
            }
            break;
        case CTRL_KEY('k'):

            if (E.numrows > 0){
                erow *row = &E.row[E.cy];
                if (E.cx < row->size){
                    row->chars[E.cx] = '\0';
                    row->size = E.cx;
                    E.dirty = 1;
                    buffer_changed = 1;
                } else if (E.cx == row->size && E.cy < E.numrows - 1){
                    E.cy++;
                    E.cx = 0;
                    editorDeleteChar();
                    buffer_changed = 1;
                }
            }
            break;

        case CTRL_KEY('b'):
            editorMoveCursor(ARROW_LEFT);
            break;
        case CTRL_KEY('f'):
            editorMoveCursor(ARROW_RIGHT);
            break;
        case CTRL_KEY('p'):
            editorMoveCursor(ARROW_UP);
            break;
        case CTRL_KEY('n'):
            editorMoveCursor(ARROW_DOWN);
            break;
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if (E.numrows > 0 && E.cy >= 0 && E.cy < E.numrows) {
                E.cx = E.row[E.cy].size;
            } else {
                E.cx = 0;
            }
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
                autocompleteSelectPrev();
            } else {
                editorMoveCursor(ARROW_UP);
            }
            break;
        case ARROW_DOWN:
            if (autocompleteIsActive()) {
                autocompleteSelectNext();
            } else {
                editorMoveCursor(ARROW_DOWN);
            }
            break;
        case ARROW_LEFT:
            if (!autocompleteIsActive()){
                editorMoveCursor(ARROW_LEFT);
            }
            break;
        case ARROW_RIGHT:
            if (!autocompleteIsActive()) {
                editorMoveCursor(ARROW_RIGHT);
            }
            break;
        case NEWLINE_KEY:
            if (autocompleteIsActive()){
                autocompleteHideSuggestions();
            }
            editorInsertNewline();
            buffer_changed = 1;
            break;
        case BACKSPACE:
            editorDeleteChar();
            buffer_changed = 1;
            break;
        case TAB_KEY:
            if (autocompleteIsActive()){
                autocompleteAcceptSuggestion();
                buffer_changed = 1;
            } else {
                erow* row = &E.row[E.cy];
                int start = E.cx;
                while (start > 0) {
                    unsigned char ch = (unsigned char) row->chars[start - 1];
                    if (!(isalnum(ch) || ch == '_')) break;
                    start--;
                }


                int wordLen = E.cx - start;
                char word[MAX_WORD_LENGTH];
                if (wordLen > 0 && wordLen < MAX_WORD_LENGTH){
                    memcpy(word, &row->chars[start], wordLen);
                    word[wordLen] = '\0';
                    syntaxReparseFull();
                    if (E.debug_tree) syntaxDebugDumpTree();
                    did_reparse = 1;
                    autocompleteUpdateSuggestions(word, E.cy, E.cx);
                    autocompleteShowSuggestions();
                }
            }
            break;
        default:
            if (!isprint((unsigned char)c)) {
                break;  // ignore stray control characters
            }

            editorInsertChar(c);
            buffer_changed = 1;

            // after inserting, recompute current word and update suggestions
            erow* row = &E.row[E.cy];
            int start = E.cx;
            while (start > 0) {
                unsigned char ch = (unsigned char) row->chars[start - 1];
                if (!(isalnum(ch) || ch == '_')) break;
                start--;
            }

            int wordLen = E.cx - start;
            char word[MAX_WORD_LENGTH];
            if (wordLen >= 2 && wordLen < MAX_WORD_LENGTH){
                memcpy(word, &row->chars[start], wordLen);
                word[wordLen] = '\0';
                syntaxReparseFull();
                if (E.debug_tree) syntaxDebugDumpTree();
                did_reparse = 1;
                autocompleteUpdateSuggestions(word, E.cy, E.cx);
                autocompleteShowSuggestions();
            } else if (autocompleteIsActive()) {
                autocompleteHideSuggestions();
            }
            break;
    }

    if (buffer_changed && !did_reparse) {
        syntaxReparseFull();
        if (E.debug_tree) syntaxDebugDumpTree();
        buffer_changed = 0;
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
    HighlightSpan spans[1024];
    int nspans = syntaxQueryVisible(E.rowoff, E.rowoff + E.screenrows - 1, spans, 1024);

    for (y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
        if (filerow >= E.numrows) {
            abAppend(ab, "~\x1b[K\r\n", 6);
            continue;
        }

        erow *row = &E.row[filerow];
        int len = row->size - E.coloff;
        if (len < 0) len = 0;
        if (len > E.screencols) len = E.screencols;

        // Reset to default color at the start of each line
        abAppend(ab, "\x1b[39m", 5);

        int last_color = 39;
        for (int i = 0; i < len; i++) {
            // Find color for this character
            int color = 39;
            for (int s = 0; s < nspans; s++) {
                if (spans[s].row == filerow &&
                    i + E.coloff >= spans[s].start_col &&
                    i + E.coloff < spans[s].end_col) {
                    color = spans[s].color_id;
                    break;
                }
            }

            if (color != last_color) {
                char buf[32];
                if (color >= 0){
                    snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                } else {
                    int palette = -color;
                    snprintf(buf, sizeof(buf), "\x1b[38;5;%dm", palette);
                }
                abAppend(ab, buf, strlen(buf));
                last_color = color;
            }

            int screen_col = i + E.coloff;

            int in_search = 0;
            if (E.search_active && E.search_match_row == filerow && E.search_match_col >= 0){
                int start = E.search_match_col;
                int end = start + E.search_match_len;
                if (screen_col >= start && screen_col < end){
                    in_search = 1;
                }
            }

            if (in_search) {
                abAppend(ab, "\x1b[48;5;238m", 11);
            }
            abAppend(ab, &row->chars[screen_col], 1);

            if (in_search) {
                abAppend(ab, "\x1b[49m", 5); // reset
            }
        }

        abAppend(ab, "\x1b[39m", 5);  // reset color
        abAppend(ab, "\x1b[K\r\n", 5);
    }
}


void editorDrawStatusBar(struct abuf *ab) {
    abAppend(ab, "\x1b[7m", 4); // invert colors
    char status[80];
    int len;
    if (E.goto_active) {
        const char *prefix = "Go to line (current ";
        len = 0;

        abAppend(ab, prefix, (int)strlen(prefix));
        len += (int)strlen(prefix);

        char num_buf[16];
        int num_len = int_to_str(E.cy + 1, num_buf);
        abAppend(ab, num_buf, num_len);
        len += num_len;

        abAppend(ab, "): ", 3);
        len += 3;

        abAppend(ab, E.goto_buf, E.goto_len);
        len += E.goto_len;

        while (len < E.screencols) { abAppend(ab, " ", 1); len++; }
        abAppend(ab, "\x1b[m", 3);
        return;
    }

    if (E.search_active){
        len = snprintf(status, sizeof(status), "Search %s (ESC to cancel)", E.search_query);
    } else {
        len = snprintf(status, sizeof(status), "L%d %.20s - %d lines %s", E.cy + 1,
                       E.filename, E.numrows, E.dirty ? "(modified)" : "");
    }
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
    E.debug_tree = 0;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
  E.screenrows -= 1;
  autocompleteInit();
}
