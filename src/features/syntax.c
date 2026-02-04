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

//static const char *k_ident_query = "(identifier) @id";

// Tier 1: locals + params
static const char *k_query_locals =
"(parameter_declaration declarator: (identifier) @id) "
"(declaration declarator: (init_declarator declarator: (identifier) @id))";



// // Tier 2: struct/union fields
// static const char *k_query_fields = 
// "(field_declaration declarator: (field_identifier) @field_id)";

// // Tier 3: file-scope globals/types/functions/enums
// static const char *k_query_globals =
// "(declaration declarator: (init_declarator declarator: (identifier) @global_id))"
// "(type_definition declarator: (type_identifier) @global_id)"
// "(function_definition declarator: (function_declarator declarator: (identifier) @global_id))"
// "(enum_specifier name: (identifier) @global_id)"
// "(preproc_def name: (identifier) @global_id)";

// static const char *k_query_locals = "(identifier) @id";
static const char *k_query_fields = "(field_identifier) @id";
static const char *k_query_globals = "(identifier) @id";


// full source code texts with lines separated by \n and len of buffer
static char *g_full_text = NULL;
static size_t g_full_len = 0;

// Byte offset at the start of each row (converting from editor rows, cols to byte mapping in flat buffer)
static size_t *g_row_byte_offsets = NULL;
static int g_row_offsets_count = 0;

// Function to debug syntax tree
void syntaxDebugDumpTree(void) {
    if (!g_tree) return;
    TSNode root = ts_tree_root_node(g_tree);
    char *tree_str = ts_node_string(root);
    FILE *f = fopen("debug_tree.txt", "w");
    if (f) {
        fputs(tree_str, f);
        fclose(f);
    }
    free(tree_str);
}


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
    return ts_node_descendant_for_byte_range(root, b, b);
}


static int collect_ids(TSNode scope, const char *qsrc,
                        const char *prefix,
                        char out[][MAX_WORD_LENGTH], int max_out, uint32_t cursor_byte)
{
    if (ts_node_is_null(scope)) return 0;

    TSQueryError err;
    uint32_t err_offset = 0;
    TSQuery *q = ts_query_new(g_lang, qsrc, (uint32_t)strlen(qsrc), &err_offset, &err);

    if (!q) {
        fprintf(stderr, "query err=%d offset=%u\n", err, err_offset);
        return 0;
    }


    TSQueryCursor *cur = ts_query_cursor_new();
    ts_query_cursor_exec(cur, q, scope);

    int count = 0;
    TSQueryMatch m;
    while (ts_query_cursor_next_match(cur, &m) && count < max_out){
        for (uint32_t i = 0; i < m.capture_count; i++){
            TSNode id = m.captures[i].node;
            uint32_t s = ts_node_start_byte(id);
            uint32_t e = ts_node_end_byte(id);
            if (cursor_byte >= s && cursor_byte < e) continue;
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

bool syntaxCursorOnDeclaratorName(int row, int col) {
    if (col > 0) col -= 1;
    if (!g_tree || !g_row_byte_offsets) return false;

    uint32_t b = (uint32_t) row_col_to_byte(row, col);
    TSNode root = ts_tree_root_node(g_tree);
    TSNode node = ts_node_descendant_for_byte_range(root, b, b);

    TSNode cur = node;
    while (!ts_node_is_null(cur)) {
        if (strcmp(ts_node_type(cur), "init_declarator") == 0) {
            TSNode decl = ts_node_child_by_field_name(cur, "declarator", 10);
            if (!ts_node_is_null(decl)) {
                uint32_t ds = ts_node_start_byte(decl);
                uint32_t de = ts_node_end_byte(decl);
                return b >= ds && b < de; // only block if on the name
            }
            return false;
        }
        cur = ts_node_parent(cur);
    }
    return false;
}



int syntaxInit(const char *lang_name, const char *query_path){
    (void) lang_name;
    g_parser = ts_parser_new();
    if (!g_parser) return -1;

    g_lang = tree_sitter_c();
    if (!g_lang) return -1;

    bool set_result = ts_parser_set_language(g_parser, g_lang);
    if (!set_result) {
        printf("language could not be set");
        return -2;
    }

    size_t qlen = 0;
    char *qsrc = read_file_to_string(query_path, &qlen);
    if (!qsrc) return -2;

    uint32_t err_offset = 0;
    TSQueryError err_type = 0;
    g_query = ts_query_new(g_lang, qsrc, (uint32_t) qlen, &err_offset, &err_type);

    if (!g_query) {
        free(qsrc);
        return -3;
    }

    free(qsrc);

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
    if (g_tree) ts_tree_delete(g_tree);
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

    size_t start_byte = row_col_to_byte(first_row, 0);
    size_t end_byte = row_col_to_byte(last_row, E.row[last_row].size);

    ts_query_cursor_set_byte_range(g_cursor, (uint32_t) start_byte, (uint32_t) end_byte);
    ts_query_cursor_exec(g_cursor, g_query, ts_tree_root_node(g_tree));

    int count = 0;
    TSQueryMatch match;

    while (ts_query_cursor_next_match(g_cursor, &match)){

        for (uint32_t i = 0; i < match.capture_count; i++){
            TSQueryCapture cap = match.captures[i];

            // ---- SAFETY CHECK START ----
            uint32_t capture_count = ts_query_capture_count(g_query);
            if (cap.index >= capture_count) {
                continue;
            }

            uint32_t name_len = 0;
            const char *cap_name = ts_query_capture_name_for_id(g_query, cap.index, &name_len);
            if (!cap_name) {
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
                                        char out[][MAX_WORD_LENGTH])
{
    if (col > 0) col -= 1;
    if (!g_tree || !g_row_byte_offsets || !g_full_text) return 0;
    if (!prefix) prefix = "";

    uint32_t b = (uint32_t) row_col_to_byte(row, col);
    TSNode root = ts_tree_root_node(g_tree);
    TSNode node = node_at_byte(root, b);
    TSNode func = find_enclosing_type(node, "function_definition");
    TSNode strt = find_enclosing_type(node, "struct_specifier");
    TSNode un = find_enclosing_type(node, "union_specifier");

    char locals[MAX_SUGGESTIONS][MAX_WORD_LENGTH];
    char str_fields[MAX_SUGGESTIONS][MAX_WORD_LENGTH];
    char un_fields[MAX_SUGGESTIONS][MAX_WORD_LENGTH];
    char globals[MAX_SUGGESTIONS][MAX_WORD_LENGTH];
    int out_count = 0;

    int n1 = collect_ids(func, k_query_locals, prefix, locals, MAX_SUGGESTIONS, b);
    int n2 = collect_ids(strt, k_query_fields, prefix, str_fields, MAX_SUGGESTIONS, b);
    int n3 = collect_ids(un, k_query_fields, prefix, un_fields, MAX_SUGGESTIONS, b);
    int n4 = collect_ids(root, k_query_globals, prefix, globals, MAX_SUGGESTIONS, b);

    for (int i = 0; i < n1 && out_count < MAX_SUGGESTIONS; i++){
        append_unique(out, MAX_SUGGESTIONS, &out_count, locals[i]);
    }
    for (int i = 0; i < n2 && out_count < MAX_SUGGESTIONS; i++){
        append_unique(out, MAX_SUGGESTIONS, &out_count, str_fields[i]);
    }
    for (int i = 0; i < n3 && out_count < MAX_SUGGESTIONS; i++){
        append_unique(out, MAX_SUGGESTIONS, &out_count, un_fields[i]);
    }
    for (int i = 0; i < n4 && out_count < MAX_SUGGESTIONS; i++){
        append_unique(out, MAX_SUGGESTIONS, &out_count, globals[i]);
    }

    return out_count;

}


void syntaxFree(void) {
    if (g_cursor) ts_query_cursor_delete(g_cursor), g_cursor = NULL;
    if (g_query)  ts_query_delete(g_query), g_query = NULL;
    if (g_tree)   ts_tree_delete(g_tree), g_tree = NULL;
    if (g_parser) ts_parser_delete(g_parser), g_parser = NULL;
    free(g_full_text); g_full_text = NULL; g_full_len = 0;
    free(g_row_byte_offsets); g_row_byte_offsets = NULL; g_row_offsets_count = 0;
}
