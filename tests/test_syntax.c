#include "common.h"
#include "syntax.h"
#include <stdio.h>
#include <string.h>

static void load_buffer_from_string(const char *src) {
    // create a single-row buffer (no newlines for this test)
    E.numrows = 1;
    E.row = malloc(sizeof(erow));
    E.row[0].size = (int)strlen(src);
    E.row[0].chars = malloc(E.row[0].size + 1);
    memcpy(E.row[0].chars, src, E.row[0].size);
    E.row[0].chars[E.row[0].size] = '\0';
}

int main(void) {
    // minimal editor config for syntax
    E.row = NULL;
    E.numrows = 0;

    if (syntaxInit("c", "tree-sitter-c/queries/highlights.scm") != 0) {
        fprintf(stderr, "syntaxInit failed\n");
        return 1;
    }

    load_buffer_from_string("int x = 10;");

    if (syntaxReparseFull() != 0) {
        fprintf(stderr, "syntaxReparseFull failed\n");
        return 1;
    }

    HighlightSpan spans[128];
    int n = syntaxQueryVisible(0, 0, spans, 128);
    if (n <= 0) {
        fprintf(stderr, "expected highlight spans, got %d\n", n);
        return 1;
    }

    syntaxFree();
    free(E.row[0].chars);
    free(E.row);
    return 0;
}
