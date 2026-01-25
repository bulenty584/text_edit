CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 \
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

textedit: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	find src -name '*.o' -delete

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf textedit $(BUILD_DIR)
