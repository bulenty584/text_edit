#include "common.h"
#include "terminal.h"
#include "editor.h"
#include "fileio.h"

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
