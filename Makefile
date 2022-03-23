.POSIX:
PREFIX = /usr/local

.SUFFIXES:
all:
	cc -O3 -ansi -Wall -Wextra -Wno-missing-field-initializers -pedantic -lm -lsndio bang.c -o "bang"
install:
	mkdir -p $(PREFIX)/bin
	mv bang $(PREFIX)/bin
uninstall:
	rm $(PREFIX)/bin/bang
