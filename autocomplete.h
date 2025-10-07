#ifndef AUTOCOMPLETE_H
#define AUTOCOMPLETE_H

#include "common.h"

#define MAX_SUGGESTIONS 10
#define MAX_WORD_LENGTH 256

typedef struct {
    char suggestions[MAX_SUGGESTIONS][MAX_WORD_LENGTH];
    int count;
    int selected;
    int start_row;
    int start_col;
    char current_word[MAX_WORD_LENGTH];
    bool is_active;
} AutocompleteState;

// Autocomplete functions
void autocompleteInit(void);
void autocompleteCleanup(void);
void autocompleteUpdateSuggestions(const char* word, int row, int col);
void autocompleteShowSuggestions(void);
void autocompleteHideSuggestions(void);
void autocompleteSelectNext(void);
void autocompleteSelectPrev(void);
void autocompleteAcceptSuggestion(void);
bool autocompleteIsActive(void);
void autocompleteDrawSuggestions(struct abuf *ab);

#endif
