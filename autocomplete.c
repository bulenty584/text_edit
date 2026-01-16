#include "autocomplete.h"

void autocompleteInit(void){
    E.autocomplete.count = 0;
    E.autocomplete.selected = 0;
    E.autocomplete.start_row = 0;
    E.autocomplete.start_col = 0;
    E.autocomplete.current_word[0] = '\0';
    E.autocomplete.is_active = false;
    
    dictionary = trieCreateNode();
    if (!dictionary) return;
    trieLoadDictionary(dictionary, "autocomplete/ckeys.txt");
}

void autocompleteCleanup(void){
    if (dictionary) {
        trieFree(dictionary);
        dictionary = NULL;
    }
}

void autocompleteUpdateSuggestions(const char* word, int row, int col){
    E.autocomplete.count = 0;
    E.autocomplete.selected = 0;
    E.autocomplete.is_active = false;

    if (!word || strlen(word) < 2 || !dictionary) return;

    strncpy(E.autocomplete.current_word, word, MAX_WORD_LENGTH - 1);
    E.autocomplete.current_word[MAX_WORD_LENGTH - 1] = '\0';

    E.autocomplete.start_row = row;
    E.autocomplete.start_col = col - strlen(word);
    E.autocomplete.count = trieGetSuggestions(dictionary, word, E.autocomplete.suggestions, MAX_SUGGESTIONS);

    if (E.autocomplete.count > 0){
        E.autocomplete.is_active = true;
        E.autocomplete.selected = 0;
    }
}

void autocompleteShowSuggestions(void){
    if (!E.autocomplete.is_active || E.autocomplete.count == 0) return;

}

void autocompleteHideSuggestions(void){
    E.autocomplete.is_active = false;
    E.autocomplete.count = 0;
}

void autocompleteSelectNext(void){
    if (!dictionary || E.autocomplete.count == 0) return;

    E.autocomplete.selected++;
    if (E.autocomplete.selected >= E.autocomplete.count) E.autocomplete.selected = 0;
    strncpy(E.autocomplete.current_word, E.autocomplete.suggestions[E.autocomplete.selected], MAX_WORD_LENGTH - 1);
    E.autocomplete.current_word[MAX_WORD_LENGTH - 1] = '\0';
}

void autocompleteSelectPrev(void){
    if (!dictionary || E.autocomplete.count == 0) return;
    
    if (E.autocomplete.selected == 0){
        E.autocomplete.selected = E.autocomplete.count - 1;
    } else {
        E.autocomplete.selected--;
    }

    strncpy(E.autocomplete.current_word, E.autocomplete.suggestions[E.autocomplete.selected], MAX_WORD_LENGTH - 1);
    E.autocomplete.current_word[MAX_WORD_LENGTH - 1] = '\0';
}

void autocompleteAcceptSuggestion(void){
    if (!E.autocomplete.is_active || E.autocomplete.count == 0) return;

    const char* suggestion = E.autocomplete.suggestions[E.autocomplete.selected];
    int suggestionLen = (int) strlen(suggestion);
    int wordLen = (int) strlen(E.autocomplete.current_word);

    if (E.cy < 0 || E.cy >= E.numrows) return;
    erow* row = &E.row[E.cy];

    int start = E.autocomplete.start_col;
    if (start < 0) start = 0;
    if (start > row->size) start = row->size;

    int end = start + wordLen;
    if (end > row->size) end = row->size;

    // new size = current - wordLen + suggestionLen
    int newSize = row->size - (end - start) + suggestionLen;
    char* newChars = malloc(newSize + 1);
    if (!newChars) return;

    memcpy(newChars, row->chars, start);
    memcpy(newChars + start, suggestion, suggestionLen);
    memcpy(newChars + start + suggestionLen, row->chars + end, row->size - end);

    newChars[newSize] = '\0';
    free(row->chars);
    row->chars = newChars;
    row->size = newSize;

    E.cx = start + suggestionLen;

    E.autocomplete.is_active = false;
    E.autocomplete.count = 0;
    E.dirty = 1;
}

bool autocompleteIsActive(void){
    return E.autocomplete.is_active && E.autocomplete.count > 0;
}

void autocompleteDrawSuggestions(struct abuf *ab){
    if (!autocompleteIsActive()) return;

    int drawRow = (E.autocomplete.start_row - E.rowoff) + 2;
    int drawCol = (E.autocomplete.start_col - E.coloff) + 1;

    // keep within screen bounds
    if (drawRow < 1) drawRow = 1;
    if (drawRow > E.screenrows) return;
    if (drawCol < 1) drawCol = 1;
    if (drawCol > E.screencols) drawCol = E.screencols;

    int maxToShow = E.autocomplete.count;
    if (maxToShow > MAX_SUGGESTIONS) maxToShow = MAX_SUGGESTIONS;

    for (int i = 0; i < maxToShow; i++){
        int r = drawRow + i;
        if (r > E.screenrows) break;

        char move[32];
        int col = drawCol;
        if (col < 1) col = 1;
        if (col > E.screencols) col = E.screencols;

        snprintf(move, sizeof(move), "\x1b[%d;%dH", r, col);
        abAppend(ab, move, (int)strlen(move));

        // clear to end of line
        abAppend(ab, "\x1b[K", 3);

        // select highlight
        if (i == E.autocomplete.selected){
            abAppend(ab, "\x1b[7m", 4);
        }

        const char* s = E.autocomplete.suggestions[i];
        int len = (int) strlen(s);
        if (len > E.screencols - col + 1) len = E.screencols - col + 1;
        if (len > 0) abAppend(ab, s, len);

        if (i == E.autocomplete.selected){
            abAppend(ab, "\x1b[m", 3); // reset
        }
    }
}

