#include "common.h"
#include "terminal.h"
#include "editor.h"
#include "fileio.h"
#include "syntax.h"

static int is_c_file(const char *path) {
  const char *dot = strrchr(path, '.');
  return dot && (strcmp(dot, ".c") == 0 || strcmp(dot, ".h") == 0);
}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();

  if (argc >= 2) {
      editorOpen(argv[1]);

      if (is_c_file(argv[1])) {
          if (syntaxInit("c", "tree-sitter-c/queries/highlights.scm") != 0) {
              perror("syntax init failed");
              exit(1);
          }
          syntaxReparseFull();
      }
  } else {
      E.filename = "temp.c";
      if (syntaxInit("c", "tree-sitter-c/queries/highlights.scm") != 0) {
          perror("syntax init failed");
          exit(1);
      }
      syntaxReparseFull();
  }

  while (1) {
      editorRefreshScreen();
      editorProcessKey();
  }
  return 0;
}

