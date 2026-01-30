#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdbool.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define TEXTEDITVERSION "0.0.1"
#define MAX_SUGGESTIONS 10
#define MAX_WORD_LENGTH 256

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
  ADD_CHAR_KEY,
  TAB_KEY,
  ENTER
};

/*** data structures ***/
typedef struct erow {
    int size;
    char* chars;
} erow;

typedef struct {
  char suggestions[MAX_SUGGESTIONS][MAX_WORD_LENGTH];
  int count;
  int selected;
  int start_row;
  int start_col;
  char current_word[MAX_WORD_LENGTH];
  bool is_active;
} AutoCompleteState;

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
    int dirty;
    int line_num;
    int debug_tree;
    int goto_active;
    char goto_buf[16];
    int goto_len;
    int goto_saved_cx, goto_saved_cy;
    AutoCompleteState autocomplete;

    // search state
    int search_active;
    char search_query[128];
    int search_len;
    int search_saved_cx, search_saved_cy;
    int search_match_row;
    int search_match_col;
    int search_match_len;

};

extern struct editorConfig E;

#endif
