#ifndef AUTOCOMPLETE_H
#define AUTOCOMPLETE_H

#include "common.h"
#include "autocomplete/Trie.h"
#include "common.h"
#include "buffer.h"
#include "syntax.h"

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
