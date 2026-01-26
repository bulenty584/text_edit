#ifndef SYNTAX_H
#define SYNTAX_H

#include "common.h"

typedef struct {
    int row;
    int start_col;
    int end_col;
    int color_id;
} HighlightSpan;

// init tree-sitter
int syntaxInit(const char *lang_name, const char *query_path);

// reparse the entire buffer (after edits)
int syntaxReparseFull(void);

// collect highlight spans for visible rows [first_row, last_row] inclusive
// returns number of spans written to spans_out (up to max_spans)
int syntaxQueryVisible(int first_row, int last_row, HighlightSpan *spans_out, int max_spans);

// collect identifiers in scope for autocomplete
int syntaxCollectIdentifiersInScope(const char *prefix, int row, int col,
                                    char out[][MAX_WORD_LENGTH]);

// cleanup all allocations
void syntaxFree(void);

// Map a capture name to color id
int syntaxColorForCapture(const char *capture_name);
#endif
