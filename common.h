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

/*** data structures ***/
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
    int dirty;
};

extern struct editorConfig E;

#endif
