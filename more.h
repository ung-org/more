#define _XOPEN_SOURCE 700
#include <stdio.h>

struct more_tty {
	FILE *tty;
	int lines;
	int columns;
};

struct more_file {
	FILE *f;
	FILE *backing;
	size_t topline;
	fpos_t *lines;
	size_t nlines;
	size_t mark[26];
	char *buf;
	size_t nbuf;
};

struct more_tty more_open_tty(int lines);
struct more_file more_open(const char *path);
void more_close(struct more_file *mf);
ssize_t more_getline(struct more_file *mf, size_t lineno);
