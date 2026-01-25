#include "fileio.h"
#include "terminal.h"

/*** file i/o functions ***/

void editorFree(void) {
    for (int i = 0; i < E.numrows; i++) {
        free(E.row[i].chars);
    }
    free(E.row);
}

void editorSave(void) {
  if (E.numrows == 0) return;
  
  FILE *fp = fopen(E.filename, "w");
  if (!fp) die("fopen");
  
  for (int i = 0; i < E.numrows; i++) {
    fwrite(E.row[i].chars, E.row[i].size, 1, fp);
    if (i < E.numrows - 1) {
      fwrite("\n", 1, 1, fp);
    }
  }
  E.dirty = 0;
  
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
  E.dirty = 0;
  free(line);
  fclose(fp);
  
  // If no lines were read, create an empty first line
  if (E.numrows == 0) {
    // We need to call editorAllocateNewRow from editor.c
    // For now, we'll do it inline to avoid circular dependencies
    E.row = malloc(sizeof(erow));
    E.row[0].size = 0;
    E.row[0].chars = malloc(1);
    E.row[0].chars[0] = '\0';
    E.numrows = 1;
    E.cy = 0;
    E.cx = 0;
  }
}
