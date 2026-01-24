#include "common.h"
#include "terminal.h"
#include "editor.h"
#include "fileio.h"
#include "syntax.h"

int main(int argc, char *argv[]) {
  // Debug: write at start
  FILE *f = fopen("debug.txt", "w");
  if (f) {
    fprintf(f, "=== TEXTEDIT STARTED ===\n");
    fclose(f);
  }

  enableRawMode();
  initEditor();

  // Initialize syntax highlighting for C code
  int d = syntaxInit("c", "tree-sitter-c/queries/highlights.scm");
  if (d != 0) {
    perror("syntax init failed");
    exit(1);
  }

  printf("d code: %d\n", d);

  if (argc >= 2) {
    editorOpen(argv[1]);
    syntaxReparseFull();

  } else {
    // HARDCODED FILENAME
    E.filename = "temp.c";
    syntaxReparseFull();
  }

  while (1) {
    editorRefreshScreen();
    editorProcessKey();
  }
  return 0;
}
