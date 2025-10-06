#ifndef TERMINAL_H
#define TERMINAL_H

#include "common.h"

/*** terminal functions ***/
void die(const char *s);
void disableRawMode(void);
void enableRawMode(void);
int editorReadKey(void);
int getCursorPosition(int *rows, int *cols);
int getWindowSize(int *rows, int *cols);

#endif
