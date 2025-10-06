#ifndef EDITOR_H
#define EDITOR_H

#include "common.h"
#include "buffer.h"

/*** editor functions ***/
void editorAllocateNewRow(void);
void editorInsertChar(int c);
void editorDeleteChar(void);
void editorInsertNewline(void);
void editorMoveCursor(int key);
void editorProcessKey(void);
void editorScroll(void);
void editorDrawRows(struct abuf *ab);
void editorDrawStatusBar(struct abuf *ab);
void editorRefreshScreen(void);
void initEditor(void);

#endif
