CC=gcc
kilo: kilo.c
	$(CC) kilo.c -o kilo -Wall -Wextra -pedantic -std=c99

.PHONY: clean

clean:
	rm -rf kilo
