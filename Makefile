CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=c99 -Itree-sitter/lib/include -Itree-sitter-c/src

# Object files
OBJS=main.o common.o terminal.o buffer.o editor.o fileio.o autocomplete.o Trie.o syntax.o ts_runtime.o ts_c_parser.o

kilo: $(OBJS)
	$(CC) $(OBJS) -o kilo $(CFLAGS)

# Individual object file rules
main.o: main.c common.h terminal.h editor.h fileio.h
	$(CC) -c main.c $(CFLAGS)

common.o: common.c common.h
	$(CC) -c common.c $(CFLAGS)

terminal.o: terminal.c terminal.h common.h
	$(CC) -c terminal.c $(CFLAGS)

buffer.o: buffer.c buffer.h common.h
	$(CC) -c buffer.c $(CFLAGS)

editor.o: editor.c editor.h common.h buffer.h terminal.h fileio.h autocomplete.h syntax.h
	$(CC) -c editor.c $(CFLAGS)

fileio.o: fileio.c fileio.h common.h terminal.h
	$(CC) -c fileio.c $(CFLAGS)

autocomplete.o: autocomplete.c autocomplete.h autocomplete/Trie.h common.h
	$(CC) -c autocomplete.c $(CFLAGS)

Trie.o: autocomplete/Trie.c autocomplete/Trie.h
	$(CC) -c autocomplete/Trie.c $(CFLAGS)

# Tree-sitter runtime and grammar objects
syntax.o: syntax.c syntax.h common.h
	$(CC) -c syntax.c $(CFLAGS)

ts_runtime.o: tree-sitter/lib/src/lib.c
	$(CC) -c tree-sitter/lib/src/lib.c $(CFLAGS) -o ts_runtime.o

ts_c_parser.o: tree-sitter-c/src/parser.c
	$(CC) -c tree-sitter-c/src/parser.c $(CFLAGS) -o ts_c_parser.o

.PHONY: clean

clean:
	rm -rf kilo $(OBJS)
