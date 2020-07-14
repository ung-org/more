#define _XOPEN_SOURCE 700
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include "more.h"

static FILE *resetty = NULL;
static struct termios resettings = { 0 };

static void resetterm(void)
{
	tcsetattr(fileno(resetty), TCSANOW, &resettings);
}

static int query_term(const char *cap, const char *variable, int def)
{
	int n = 0;
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "tput %s", cap);
	FILE *f = popen(cmd, "r");
	if (f) {
		if (fscanf(f, "%d", &n) != 1) {
			n = 0;
		}
		pclose(f);
		if (n != 0) {
			return n;
		}
	}

	char *value = getenv(variable);
	if (value) {
		n = atoi(value);
	}
	return n ? n : def;
}

struct more_tty more_open_tty(int lines)
{
	struct more_tty mt = {
		.tty = stderr,
		.lines = query_term("lines", "LINES", 24),
		.columns = query_term("cols", "COLUMNS", 80),
	};

	if (lines > 0) {
		mt.lines = lines;
	}

	/* leave room for prompts */
	lines--;

	/* FIXME: only open /dev/tty if stderr is not readable */
	// if (!(fcntl(fileno(mt.tty), F_GETFL) & (O_WRONLY | O_RDWR))) {
		mt.tty = fopen("/dev/tty", "rb+");
	// }
	if (mt.tty == NULL) {
		perror("Couldn't open tty for reading");
		exit(1);
	}

	setbuf(mt.tty, NULL);

	tcgetattr(fileno(mt.tty), &resettings);
	struct termios term = resettings;
	term.c_lflag &= ~(ECHO | ICANON);
	term.c_cc[VMIN] = 1;
	term.c_cc[VTIME] = 0;
	tcsetattr(fileno(mt.tty), TCSANOW, &term);

	resetty = mt.tty;
	atexit(resetterm);

	return mt;
}
