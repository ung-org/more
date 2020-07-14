#define _XOPEN_SOURCE 700
#include <stdio.h>

struct more_tty {
	FILE *tty;
	int lines;
	int columns;
};

struct morefile {
	FILE *f;
	FILE *backing;
	size_t topline;
	fpos_t *lines;
	size_t nlines;
	size_t mark[26];
};

struct more_tty more_open_tty(int lines);
