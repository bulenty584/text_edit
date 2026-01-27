CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 \
        -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE \
        -Itree-sitter/lib/include \
        -Itree-sitter-c/src -g -O0 \
        -I src/include \
        -I src/core \
        -I src/io \
        -I src/features \
        -I src/features/autocomplete

LDFLAGS =

SRCS = src/main.c \
        src/include/common.c \
        src/io/terminal.c \
        src/core/buffer.c \
        src/core/editor.c \
        src/io/fileio.c \
        src/features/autocomplete.c \
        src/features/autocomplete/Trie.c \
        src/features/syntax.c \
        tree-sitter/lib/src/lib.c \
        tree-sitter-c/src/parser.c

BUILD_DIR = build
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)
TEST_SRCS = tests/test_parser.c tests/test_syntax.c
TEST_BINS = $(TEST_SRCS:tests/%.c=$(BUILD_DIR)/tests/%)

$(BUILD_DIR)/tests/%: tests/%.c \
    src/include/common.c \
    src/features/syntax.c \
    tree-sitter/lib/src/lib.c \
    tree-sitter-c/src/parser.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@


textedit: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	find src -name '*.o' -delete

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean test
clean:
	rm -rf textedit $(BUILD_DIR)

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		echo "Running $$t"; \
		$$t || exit 1; \
	done

