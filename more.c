#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

enum {
	CTRL_B = 0x02,
	CTRL_D = 0x04,
	CTRL_F = 0x06,
	CTRL_G = 0x07,
	CTRL_L = 0x0c,
	CTRL_U = 0x15,
};

struct {
	FILE *tty;
	struct termios original;
	int lines;
	int columns;
	int ret;
} global = {0};

struct morefile {
	FILE *f;
	int topline;
	char **lines;
	size_t nlines;
	char *pattern;
	int mark[26];
};

void resetterm(void)
{
	tcsetattr(fileno(global.tty), TCSANOW, &global.original);
}

void openrawtty(void)
{
	struct termios term;

	global.tty = stderr;
	/* FIXME */
	/* if (!(fcntl(fileno(global.tty), F_GETFL) & (O_WRONLY | O_RDWR))) { */
		global.tty = fopen("/dev/tty", "rb+");
	/* } */
	if (global.tty == NULL) {
		perror("Couldn't open tty for reading");
		exit(1);
	}

	setbuf(global.tty, NULL);

	tcgetattr(fileno(global.tty), &global.original);
	term = global.original;
	term.c_lflag &= ~(ECHO | ICANON);
	term.c_cc[VMIN] = 1;
	term.c_cc[VTIME] = 0;
	tcsetattr(fileno(global.tty), TCSANOW, &term);
	atexit(resetterm);
}

void refresh(struct morefile *mf)
{
	char *line = NULL;
	size_t n = 0;
	for (int i = 0; i < global.lines; i++) {
		if (getline(&line, &n, mf->f) == -1) {
			break;
		}
		printf("%s", line);
	}
	free(line);
}

void scroll(struct morefile *mf, int count, int multiple)
{
	char *line = NULL;
	size_t n = 0;
	int total = count ? count * multiple : multiple;
	for (int i = 0; i < total; i++) {
		ssize_t nread = getline(&line, &n, mf->f);
		if (nread <= 0) {
			break;
		}
		printf("%s", line);
		mf->topline++;

		/* FIXME: doesn't account for tabs */
		while (nread > global.columns) {
			i++;
			nread -= global.columns;
		}
	}
	free(line);
}

void mark(struct morefile *mf)
{
	int c = fgetc(global.tty);
	if (islower(c)) {
		mf->mark[c - 'a'] = mf->topline;
	}
}

void jump(struct morefile *mf)
{
	int c = fgetc(global.tty);
	if (islower(c)) {
		mf->topline = mf->mark[c - 'a'];
		refresh(mf);
	}
}

int more(const char *file)
{
	struct morefile mf = { stdin, 0 };
	int count = 0;
	char *line = NULL;
	size_t nline = 0;

	if (strcmp(file, "-")) {
		mf.f = fopen(file, "r");
		if (!mf.f) {
			fprintf(stderr, "more: %s: %s\n", file, strerror(errno));
			global.ret = 1;
			return 1;
		}
	}

	if (global.tty == NULL) {
		/* tty is never opened if stdout is not a tty */
		int blank = 0;
		while (getline(&line, &nline, mf.f) != -1) {
			/* if (!((global.flags & FLAG_S) && blank)) { */
				printf("%s", line);
			/* } */
			blank = !strcmp(line, "\n");
		}
		fclose(mf.f);
		return 1;
	}

	refresh(&mf);
	while (mf.f) {
		int c = fgetc(global.tty);

		switch (c) {
			case EOF:
				exit(2);
				break;

			case 'h':
				//show_help();
				printf("Help!");
				break;

			case 'f':
			case CTRL_F:
				scroll(&mf, count, 1);
				break;

			case 'b':
			case CTRL_B:
				scroll(&mf, count, -1);
				break;

			case ' ':
				count = count ? count : global.lines;
				/* FALLTHRU */
			case 'j':
			case '\n':
				scroll(&mf, count, 1);
				break;

			case 'k':
				scroll(&mf, count, -1);
				break;

			case 'd':
			case CTRL_D:
				scroll(&mf, count, global.lines / 2);
				break;

			case 's':
				count = count ? count : 1;
				scroll(&mf, global.lines + count, 1);
				break;

			case 'u':
			case CTRL_U:
				scroll(&mf, count, -global.lines / 2);
				break;

			case 'g':
				//scroll_beginning(count);
				break;

			case 'G':
				//scroll_end(count);
				break;

			case 'r':
			case CTRL_L:
				refresh(&mf);
				break;

			case 'R':
				// discard();
				refresh(&mf);
				break;

			case 'm':
				mark(&mf);
				break;

			case '\'':
				jump(&mf);
				break;

			case '/':
				//search(count);
				break;

			case '?':
				//search(-count);
				break;

			case 'n':
				//repeatsearch(count);
				break;

			case 'N':
				//repeatsearch(-count);
				break;

			case ':': {
				fputc(c, global.tty);
				int c2 = fgetc(global.tty);
				fprintf(global.tty, "\b \b");
				switch (c2) {
				case 'e':
					// examine();
					break;

				case 'n':
					return count ? count : 1;

				case 'p':
					return count ? -count : -1;

				case 't':
					// tagstring();
					break;

				case 'q':
					exit(0);

				default:
					break;
				}
			}

			case 'v':
				// invoke_editor();
				break;

			case '=':
			case CTRL_G:
				// display_position();
				break;

			case 'Z':
				if (fgetc(global.tty) != 'Z') {
					break;
				}
				/* FALLTHRU */
			case 'q':
				exit(0);

			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				count = count * 10 + (c - '0');
				break;

			default:
				break;
		}
		if (!isdigit(c)) {
			count = 0;
		}
	}

	return 0;
}

static int query_term(const char *cap, const char *variable, int def)
{
	int n = 0;
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "tput %s", cap);
	FILE *f = popen(cmd, "r");
	if (f) {
		fscanf(f, "%d", &n);
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

static void adjust_args(int *argc, char ***argv)
{
	char *env = getenv("MORE");
	if (env) {
		char **newargv = malloc((*argc + 2) * sizeof(*newargv));
		newargv[0] = *argv[0];
		
		/* TODO: account for spaces in env */
		newargv[1] = env;

		for (int i = 1; i < *argc; i++) {
			newargv[i + 1] = *argv[i];
		}

		*argv = newargv;
		*argc++;
	}
	
	for (int i = 1; i < *argc; i++) {
		if (!strcmp(**argv, "--")) {
			return;
		}

		if (**argv[0] == '+') {
			**argv[0] = '-';
		}
	}
}

int main(int argc, char *argv[])
{
	int c;
	global.lines = query_term("lines", "LINES", 24);
	global.columns = query_term("cols", "COLUMNS", 80);

	int clear = 0;
	int fastexit = 0;
	int ignorecase = 0;
	int compressempty = 0;
	int backspace = 1;

	adjust_args(&argc, &argv);

	while ((c = getopt(argc, argv, "ceisun:p:t:")) != -1) {
		switch (c) {
		case 'c':
			clear = 1;
			break;

		case 'e':
			fastexit = 1;
			break;

		case 'i':
			ignorecase = 1;
			break;

		case 's':
			compressempty = 1;
			break;

		case 'u':
			backspace = 0;
			break;

		case 'n':
			global.lines = atoi(optarg);
			break;

		case 'p':
			//global.perfile = optarg;
			break;

		case 't':
			//global.tag = optarg;
			break;

		default:
			return 1;
		}
	}

	if (isatty(STDOUT_FILENO)) {
		openrawtty();
	}

	global.lines--;

	if (optind >= argc) {
		more("-");
	}

	int min = optind;
	int max = argc - 1;
	while (optind < argc) {
		int next = more(argv[optind]);
		optind += next;
		if (optind < min) {
			optind = min;
		}
		if (optind > max) {
			optind = max;
		}
	}

	return global.ret;
}
