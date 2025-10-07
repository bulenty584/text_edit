#include <tree_sitter/api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "syntax.h"

extern const TSLanguage *tree_sitter_c(void);

static TSParser         *g_parser = NULL;
static TSTree           *g_tree = NULL;
static TSQuery          *g_query = NULL;
static TSQueryCursor    *g_cursor = NULL;
static const TSLanguage *g_lang = NULL;

// full source code texts with lines separated by \n and len of buffer
static char *g_full_text = NULL;
static size_t g_full_len = 0;

// Byte offset at the start of each row (converting from editor rows, cols to byte mapping in flat buffer)
static size_t *g_row_byte_offsets = NULL;
static int g_row_offsets_count = 0;

// read file into memory (for highlights.scm)
static char *read_file_to_string(const char *path, size_t *output_len){
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    if (sz < 0){
        fclose(fp);
        return NULL;
    }
    
    fseek(fp, 0, SEEK_SET);

    char *buf = malloc((size_t) sz + 1);
    if (!buf){ fclose(fp); return NULL;}
    size_t n = fread(buf, 1, (size_t) sz, fp);
    fclose(fp);
    buf[n] = '\0';
    if (output_len) *output_len = n;
    return buf;
}

// build g_full_text and g_row_byte_offsets from E
static void rebuild_full_text(void){
    //free old
    free(g_full_text); g_full_text = NULL; g_full_len = 0;
    free(g_row_byte_offsets); g_row_byte_offsets = NULL; g_row_offsets_count = 0;

    if (E.numrows <= 0){
        g_full_text = strdup("");
        g_full_len = 0;
        g_row_offsets_count = 0;
        return;
    }

    // total bytes with \n joins
    size_t total = 0;
    for (int i = 0; i < E.numrows; i++) total += (size_t) E.row[i].size + 1; // +1 for \n

    if (total > 0) total -= 1; // get rid of newline

    g_full_text = malloc(total + 1);
    if (!g_full_text) { g_full_len = 0; return;}

    g_row_byte_offsets = malloc(sizeof(size_t) * (size_t)E.numrows);
    if (!g_row_byte_offsets) { free(g_full_text); g_full_len = 0; return;}

    size_t pos = 0;

    for (int i = 0; i < E.numrows; i++){
        g_row_byte_offsets[i] = pos;
        memcpy(g_full_text + pos, E.row[i].chars, (size_t)E.row[i].size);
        pos += (size_t) E.row[i].size;
        if (i < E.numrows - 1){
            g_full_text[pos++] = '\n';
        }
    }

    g_full_text[pos] = '\0';
    g_full_len = pos;
    g_row_offsets_count = E.numrows;
}

// convert (row, col) -> byte offset
static size_t row_col_to_byte(int row, int col){
    if (row < 0 || row >= g_row_offsets_count) return 0;

    size_t base = g_row_byte_offsets[row];
    size_t line_len = (size_t) E.row[row].size;
    if (col < 0) col = 0;
    if ((size_t) col > line_len) col = (int) line_len;
    return base + (size_t) col;
}

int syntaxInit(const char *lang_name, const char *query_path){
    (void) lang_name;
    g_parser = ts_parser_new();
    if (!g_parser) return -1;

    g_lang = tree_sitter_c();
    ts_parser_set_language(g_parser, g_lang);

    size_t qlen = 0;
    char *qsrc = read_file_to_string(query_path, &qlen);
    if (!qsrc) return -2;

    uint32_t err_offset = 0;
    TSQueryError err_type = 0;
    g_query = ts_query_new(g_lang, qsrc, (uint32_t) qlen, &err_offset, &err_type);

    free(qsrc);
    if (!g_query) return -3;

    g_cursor = ts_query_cursor_new();
    if (!g_cursor) return -4;

    rebuild_full_text();
    g_tree = ts_parser_parse_string(g_parser, NULL, g_full_text, (uint32_t) g_full_len);
    return (g_tree ? 0 : -5);


}

