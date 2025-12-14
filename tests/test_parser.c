#include <tree_sitter/api.h>
#include <stdio.h>
#include <string.h>

extern const TSLanguage *tree_sitter_c(void);

int main() {
    // Create parser
    TSParser *parser = ts_parser_new();
    const TSLanguage *lang = tree_sitter_c();

    printf("Language pointer: %p\n", (void*)lang);
    printf("Language ABI version: %u\n", ts_language_abi_version(lang));

    bool result = ts_parser_set_language(parser, lang);
    printf("Set language result: %d\n", result);

    // Parse some C code
    const char *source_code = "int main(){\n    int x;\n}";
    TSTree *tree = ts_parser_parse_string(parser, NULL, source_code, strlen(source_code));

    if (!tree) {
        printf("Failed to create tree!\n");
        return 1;
    }

    // Get root node
    TSNode root = ts_tree_root_node(tree);
    char *tree_str = ts_node_string(root);

    printf("Parse tree: %s\n", tree_str);
    printf("Root child count: %d\n", ts_node_child_count(root));
    printf("Root has error: %d\n", ts_node_has_error(root));

    free(tree_str);
    ts_tree_delete(tree);
    ts_parser_delete(parser);

    return 0;
}
