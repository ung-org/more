#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

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
	enum {
		FLAG_C = 1 << 0,
/*
If a screen is to be written that has no lines in common with the current screen, or more is writing its first screen, more shall not scroll the screen, but instead shall redraw each line of the screen in turn, from the top of the screen to the bottom. In addition, if more is writing its first screen, the screen shall be cleared. This option may be silently ignored on devices with insufficient terminal capabilities.
*/
		FLAG_E = 1 << 1,
/*
Exit immediately after writing the last line of the last file in the argument list; see the EXTENDED DESCRIPTION section.
*/
		FLAG_I = 1 << 2,
/*
Perform pattern matching in searches without regard to case; see XBD Regular Expression General Requirements.
*/
		FLAG_S = 1 << 3,
/*
Behave as if consecutive empty lines were a single empty line.
*/
		FLAG_U = 1 << 4,
/*
Treat a <backspace> as a printable control character, displayed as an implementation-defined character sequence (see the EXTENDED DESCRIPTION section), suppressing backspacing and the special handling that produces underlined or standout mode text on some terminal types. Also, do not ignore a <carriage-return> at the end of a line.
*/
	} flags;
	int lines;
/*
Specify the number of lines per screenful. The number argument is a positive decimal integer. The -n option shall override any values obtained from any other source.
*/
	int columns;
	int ret;
	char *perfile;
/*
Each time a screen from a new file is displayed or redisplayed (including as a result of more commands; for example, :p), execute the more command(s) in the command arguments in the order specified, as if entered by the user after the first screen has been displayed. No intermediate results shall be displayed (that is, if the command is a movement to a screen different from the normal first screen, only the screen resulting from the command shall be displayed.) If any of the commands fail for any reason, an informational message to this effect shall be written, and no further commands specified using the -p option shall be executed for this file.
*/
	char *tag;
/*
Write the screenful of the file containing the tag named by the tagstring argument. See the ctags utility. The tags feature represented by -t tagstring and the :t command is optional. It shall be provided on any system that also provides a conforming implementation of ctags; otherwise, the use of -t produces undefined results.
The filename resulting from the -t option shall be logically added as a prefix to the list of command line files, as if specified by the user. If the tag named by the tagstring argument is not found, it shall be an error, and more shall take no further action.

If the tag specifies a line number, the first line of the display shall contain the beginning of that line. If the tag specifies a pattern, the first line of the display shall contain the beginning of the matching text from the first line of the file that contains that pattern. If the line does not exist in the file or matching text is not found, an informational message to this effect shall be displayed, and more shall display the default screen as if -t had not been specified.

If both the -t tagstring and -p command options are given, the -t tagstring shall be processed first; that is, the file and starting line for the display shall be as specified by -t, and then the -p more command shall be executed. If the line (matching text) specified by the -t command does not exist (is not found), no -p more command shall be executed for this file at any time.
*/
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
		getline(&line, &n, mf->f);
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
			fprintf(stderr, "Couldn't open %s: %s\n", file, strerror(errno));
			global.ret = 1;
			return 1;
		}
	}

	if (global.tty == NULL) {
		/* tty is never opened if stdout is not a tty */
		int blank = 0;
		while (getline(&line, &nline, mf.f) > 0) {
			if (!((global.flags & FLAG_S) && blank)) {
				printf("%s", line);
			}
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
				scroll(&mf, count, global.lines);
				break;

			case 'b':
			case CTRL_B:
				scroll(&mf, count, -global.lines);
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

int from_env(const char *variable, int def)
{
	int n = 0;
	char *value = getenv(variable);
	if (value) {
		n = atoi(value);
	}
	return n ? n : def;
}

int main(int argc, char **argv)
{
	int c;
	global.lines = from_env("LINES", 24);
	global.columns = from_env("COLUMNS", 80);

	/* TODO: process $MORE environment variable */
	/* effective command line is more $MORE options operands */

	while ((c = getopt(argc, argv, "ceisun:p:t:")) != -1) {
		switch (c) {
		case 'c':
			global.flags |= FLAG_C;
			break;

		case 'e':
			global.flags |= FLAG_E;
			break;

		case 'i':
			global.flags |= FLAG_I;
			break;

		case 's':
			global.flags |= FLAG_S;
			break;

		case 'u':
			global.flags |= FLAG_U;
			break;

		case 'n':
			global.lines = atoi(optarg);
			break;

		case 'p':
			global.perfile = optarg;
			break;

		case 't':
			global.tag = optarg;
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
