CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 \
         -Itree-sitter/lib/include \
         -Itree-sitter-c/src
LDFLAGS =

SRCS = main.c common.c terminal.c buffer.c editor.c fileio.c \
        autocomplete.c autocomplete/Trie.c syntax.c \
        tree-sitter/lib/src/lib.c tree-sitter-c/src/parser.c
OBJS = $(SRCS:.c=.o)

textedit: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f textedit $(OBJS)
