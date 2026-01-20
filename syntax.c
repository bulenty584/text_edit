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

static const char *k_ident_query = "(identifier) @id";

static int prefix_match(const char* s, const char* prefix){
    int n = strlen(prefix);
    return n == 0 || strncmp(s, prefix, n) == 0;
}

static int append_unique(char out[][MAX_WORD_LENGTH], int max_out,
                            int *count, const char* s)
{
    for (int i = 0; i < *count; i++){
        if (strcmp(out[i], s) == 0) return 0;
    }

    if (*count >= max_out) return 0;
    strncpy(out[*count], s, MAX_WORD_LENGTH - 1);
    out[*count][MAX_WORD_LENGTH - 1] = '\0';
    (*count)++;
    return 1;
}

static TSNode find_enclosing_type(TSNode node, const char* type_name){
    while (!ts_node_is_null(node)){
        const char *type = ts_node_type(node);
        if (strcmp(type, type_name) == 0) return node;
        node = ts_node_parent(node);
    }
    TSNode null_node = {0};
    return null_node;
}

static TSNode node_at_byte(TSNode root, uint32_t b){
    TSNode cur = root;
    while (true){
        uint32_t n = ts_node_child_count(cur);
        bool advanced = false;
        for (uint32_t i = 0; i < n; i++){
            TSNode ch = ts_node_child(cur, i);
            uint32_t s = ts_node_start_byte(ch);
            uint32_t e = ts_node_end_byte(ch);
            if (b >= s && b < e){
                cur = ch;
                advanced = true;
                break;
            }
        }
        if (!advanced) break;
    }

    return cur;
}

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

    FILE *dbg = fopen("debug.txt", "a");
    if (dbg) {
        fprintf(dbg, "=== syntaxInit ===\n");
        fprintf(dbg, "Language pointer: %p\n", (void*)g_lang);
        //fprintf(dbg, "Language ABI version: %u\n", ts_language_abi_version(g_lang));
        fclose(dbg);
    }

    bool set_result = ts_parser_set_language(g_parser, g_lang);

    if (dbg) {
        dbg = fopen("debug.txt", "a");
        fprintf(dbg, "ts_parser_set_language result: %d\n", set_result);
        fclose(dbg);
    }

    printf("Language pointer: %p\n", (void *) g_lang);

    size_t qlen = 0;
    char *qsrc = read_file_to_string(query_path, &qlen);
    if (!qsrc) return -2;

    uint32_t err_offset = 0;
    TSQueryError err_type = 0;
    g_query = ts_query_new(g_lang, qsrc, (uint32_t) qlen, &err_offset, &err_type);

    if (!g_query) {
        FILE *dbg = fopen("debug.txt", "a");
        if (dbg) {
            fprintf(dbg, "Query compilation failed! err_type=%d, err_offset=%u\n", err_type, err_offset);
            fclose(dbg);
        }
        free(qsrc);
        return -3;
    }

    free(qsrc);

    // Debug: log query info
    dbg = fopen("debug.txt", "a");
    if (dbg) {
        uint32_t capture_count = ts_query_capture_count(g_query);
        uint32_t pattern_count = ts_query_pattern_count(g_query);
        fprintf(dbg, "Query compiled successfully!\n");
        fprintf(dbg, "  Capture count: %u\n", capture_count);
        fprintf(dbg, "  Pattern count: %u\n", pattern_count);
        fclose(dbg);
    }

    g_cursor = ts_query_cursor_new();
    if (!g_cursor) return -4;

    // Don't parse initially - tree will be NULL until first reparse

    return 0;


}

int syntaxReparseFull(void) {
    if (!g_parser) return -1;

    rebuild_full_text();

    // reset query cursor to ensure it uses current tree
    if (g_cursor) ts_query_cursor_delete(g_cursor);
    g_cursor = ts_query_cursor_new();
    TSTree *new_tree = ts_parser_parse_string(g_parser, NULL, g_full_text, (uint32_t)g_full_len);
    if (!new_tree) return -2;
    g_tree = new_tree;

    return 0;
}


// simple mappings of names to color ids
int syntaxColorForCapture(const char *name){
    if (strcmp(name, "comment") == 0) return 90; // gray
    if (strcmp(name, "string") == 0) return 32; // green
    if (strcmp(name, "system_lib_string") == 0) return 32; // green
    if (strcmp(name, "number") == 0) return 31; // red
    if (strcmp(name, "number_literal") == 0) return 31; // red
    if (strcmp(name, "char_literal") == 0) return 31; // red
    if (strcmp(name, "type") == 0) return 36; // cyan
    if (strcmp(name, "type_identifier") == 0) return 36; // cyan
    if (strcmp(name, "primitive_type") == 0) return 36; // cyan
    if (strcmp(name, "sized_type_specifier") == 0) return 36; // cyan
    if (strcmp(name, "keyword.typedef") == 0) return -54; // dark indigo
    if (strcmp(name, "keyword.return") == 0) return 31;
    if (strcmp(name, "keyword") == 0) return 33;  // yellow
    if (strcmp(name, "preproc_directive") == 0) return 33;  // yellow (preprocessor)
    if (strcmp(name, "function") == 0) return 34;  // blue
    if (strcmp(name, "function.special") == 0) return 34;  // blue (macros)
    if (strcmp(name, "constant") == 0) return 35;  // magenta
    if (strcmp(name, "property") == 0) return 36;  // cyan (struct fields)
    if (strcmp(name, "field_identifier") == 0) return 36;  // cyan (fields)
    if (strcmp(name, "label") == 0) return 35;  // magenta (goto labels)
    if (strcmp(name, "statement_identifier") == 0) return 35;  // magenta (labels)
    if (strcmp(name, "operator") == 0) return 37;  // white (operators)
    if (strcmp(name, "delimiter") == 0) return 37;  // white (. and ;)
    if (strcmp(name, "variable") == 0) return 39;  // default (identifiers)
    return 39; // default

}

int syntaxQueryVisible(int first_row, int last_row, HighlightSpan *spans_out, int max_spans){
    if (!g_tree || !g_query || !g_cursor || !spans_out || max_spans <= 0) return -1;

    if (first_row < 0) first_row = 0;
    if (last_row >= E.numrows) last_row = E.numrows - 1;
    if (last_row < first_row) return 0;

    // Debug: log what we're parsing (always for first 3 calls)
    static int debug_count = 0;
    debug_count++;
    if (debug_count <= 3 || (debug_count == 10)) {
        FILE *f = fopen("debug.txt", "a");
        if (f) {
            fprintf(f, "=== Call %d ===\n", debug_count);
            fprintf(f, "syntaxQueryVisible: first_row=%d, last_row=%d\n", first_row, last_row);
            fprintf(f, "  g_full_text='%s' (len=%zu)\n", g_full_text ? g_full_text : "NULL", g_full_len);
            fprintf(f, "  g_row_offsets_count=%d\n", g_row_offsets_count);
            if (E.numrows > 0 && E.row) {
                fprintf(f, "  E.row[0].chars='%s' (size=%d)\n", E.row[0].chars, E.row[0].size);
            }

            // Debug: print parse tree
            TSNode root = ts_tree_root_node(g_tree);
            char *tree_str = ts_node_string(root);
            fprintf(f, "  Parse tree: %s\n", tree_str);
            fprintf(f, "  Root node has_error: %d\n", ts_node_has_error(root));
            fprintf(f, "  Root node child_count: %d\n", ts_node_child_count(root));

            // Print first child if it exists
            if (ts_node_child_count(root) > 0) {
                TSNode first_child = ts_node_child(root, 0);
                char *child_str = ts_node_string(first_child);
                fprintf(f, "  First child: %s\n", child_str);
                free(child_str);
            }

            free(tree_str);

            fclose(f);
        }
    }

    size_t start_byte = row_col_to_byte(first_row, 0);
    size_t end_byte = row_col_to_byte(last_row, E.row[last_row].size);

    ts_query_cursor_set_byte_range(g_cursor, (uint32_t) start_byte, (uint32_t) end_byte);
    ts_query_cursor_exec(g_cursor, g_query, ts_tree_root_node(g_tree));

    int count = 0;
    TSQueryMatch match;

    // Debug: log query execution
    static int query_debug = 0;
    if (query_debug++ < 3) {
        FILE *f = fopen("debug.txt", "a");
        if (f) {
            fprintf(f, "  Query: start_byte=%zu, end_byte=%zu\n", start_byte, end_byte);
            fprintf(f, "  About to iterate matches...\n");
            fclose(f);
        }
    }

    while (ts_query_cursor_next_match(g_cursor, &match)){
        // Debug first match
        if (query_debug <= 3) {
            FILE *f = fopen("debug.txt", "a");
            if (f) {
                fprintf(f, "  Found match! capture_count=%d\n", match.capture_count);
                fclose(f);
            }
        }
        for (uint32_t i = 0; i < match.capture_count; i++){
            TSQueryCapture cap = match.captures[i];

            // Debug before crash
            if (query_debug <= 3) {
                FILE *f = fopen("debug.txt", "a");
                if (f) {
                    fprintf(f, "  Processing capture %d, index=%d\n", i, cap.index);
                    fclose(f);
                }
            }

            // ---- SAFETY CHECK START ----
            uint32_t capture_count = ts_query_capture_count(g_query);
            if (cap.index >= capture_count) {
                FILE *f = fopen("debug.txt", "a");
                if (f) {
                    fprintf(f, "  ⚠️ Invalid capture index %u (max %u) — skipping\n",
                            cap.index, capture_count);
                    fclose(f);
                }
                continue;
            }

            uint32_t name_len = 0;
            const char *cap_name = ts_query_capture_name_for_id(g_query, cap.index, &name_len);
            if (!cap_name) {
                FILE *f = fopen("debug.txt", "a");
                if (f) {
                    fprintf(f, "  ⚠️ Capture name lookup failed for id=%u\n", cap.index);
                    fclose(f);
                }
                continue;
            }
            // ---- SAFETY CHECK END ----

            int color = syntaxColorForCapture(cap_name);

            TSNode node = cap.node;

            uint32_t sbyte = ts_node_start_byte(node);
            uint32_t ebyte = ts_node_end_byte(node);

            int srow = first_row;
            int erow = last_row;

            // derive srow
            {
                int lo = first_row, hi = last_row;
                while (lo <= hi){
                    int mid = (lo + hi) / 2;
                    size_t base = g_row_byte_offsets[mid];
                    if (base <= sbyte){ srow = mid; lo = mid + 1; } else { hi = mid - 1; }
                }
            }

            // derive erow
            {
                int lo = srow, hi = last_row;
                while (lo <= hi){
                    int mid = (lo + hi) / 2;
                    size_t base = g_row_byte_offsets[mid];
                    if (base <= ebyte) { erow = mid; lo = mid + 1; } else { hi = mid - 1; }
                }
            }

            //emit spans per affected row
            for (int row = srow; row <= erow && count < max_spans; row++){
                size_t row_base = g_row_byte_offsets[row];
                size_t row_end = row_base + (size_t) E.row[row].size;

                size_t seg_start = sbyte > row_base ? sbyte : row_base;
                size_t seg_end = ebyte < row_end ? ebyte : row_end;

                if (seg_end > seg_start){
                    HighlightSpan hs;
                    hs.row = row;
                    hs.start_col = (int) (seg_start - row_base);
                    hs.end_col = (int) (seg_end - row_base);
                    hs.color_id = color;
                    spans_out[count++] = hs;
                    if (count >= max_spans) break;
                }
            }

            if (count >= max_spans) break;

        }
        if (count >= max_spans) break;
    }

    return count;
}

int syntaxCollectIdentifiersInScope(const char* prefix, int row, int col, 
                                        char out[][MAX_WORD_LENGTH], int max_out)
{
    if (!g_tree || !g_row_byte_offsets || !g_full_text) return 0;
    if (!prefix) prefix = "";

    int count = 0;
    uint32_t b = (uint32_t) row_col_to_byte(row, col);
    TSNode root = ts_tree_root_node(g_tree);
    TSNode node = node_at_byte(root, b);
    TSNode func = find_enclosing_type(node, "function_definition");

    TSQueryError err;
    uint32_t err_offset = 0;
    TSQuery *q = ts_query_new(g_lang, k_ident_query, (uint32_t)strlen(k_ident_query), &err_offset, &err);

    if (!q) return 0;

    TSQueryCursor *cur = ts_query_cursor_new();
    TSNode scope = ts_node_is_null(func) ? root : func;

    ts_query_cursor_exec(cur, q, scope);

    TSQueryMatch m;
    while (ts_query_cursor_next_match(cur, &m) && count < max_out){
        for (uint32_t i = 0; i < m.capture_count; i++){
            TSNode id = m.captures[i].node;
            uint32_t s = ts_node_start_byte(id);
            uint32_t e = ts_node_end_byte(id);
            uint32_t len = e > s ? e - s : 0;
            if (len == 0 || len >= MAX_WORD_LENGTH) continue;

            char tmp[MAX_WORD_LENGTH];
            memcpy(tmp, g_full_text + s, len);
            tmp[len] = '\0';

            if (!prefix_match(tmp, prefix)) continue;
            append_unique(out, max_out, &count, tmp);
            if (count >= max_out) break;
        }
    }

    ts_query_cursor_delete(cur);
    ts_query_delete(q);

    return count;
}


void syntaxFree(void) {
    if (g_cursor) ts_query_cursor_delete(g_cursor), g_cursor = NULL;
    if (g_query)  ts_query_delete(g_query), g_query = NULL;
    if (g_tree)   ts_tree_delete(g_tree), g_tree = NULL;
    if (g_parser) ts_parser_delete(g_parser), g_parser = NULL;
    free(g_full_text); g_full_text = NULL; g_full_len = 0;
    free(g_row_byte_offsets); g_row_byte_offsets = NULL; g_row_offsets_count = 0;
}
