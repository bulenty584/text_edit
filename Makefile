CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=c99

# Object files
OBJS=main.o common.o terminal.o buffer.o editor.o fileio.o

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

editor.o: editor.c editor.h common.h buffer.h terminal.h fileio.h
	$(CC) -c editor.c $(CFLAGS)

fileio.o: fileio.c fileio.h common.h terminal.h
	$(CC) -c fileio.c $(CFLAGS)

.PHONY: clean

clean:
	rm -rf kilo $(OBJS)
