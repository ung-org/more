#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "more.h"

static int retval = 0;

enum {
	CTRL_B = 0x02,
	CTRL_D = 0x04,
	CTRL_F = 0x06,
	CTRL_G = 0x07,
	CTRL_L = 0x0c,
	CTRL_U = 0x15,
};

static ssize_t more_getline(struct morefile *mf, size_t lineno, char **line, size_t *n)
{
	if (mf->nlines <= lineno && mf->nlines != 0) {
		fsetpos(mf->f, &(mf->lines[mf->nlines - 1]));
		getline(line, n, mf->f);
	}

	while (mf->nlines <= lineno) {
		mf->nlines++;
		mf->lines = realloc(mf->lines, mf->nlines * sizeof(*mf->lines));

		fgetpos(mf->f, &(mf->lines[mf->nlines - 1]));

		getline(line, n, mf->f);

		if (mf->backing != mf->f) {
			fgetpos(mf->backing, &(mf->lines[mf->nlines - 1]));
			fputs(*line, mf->backing);
		}
	}

	fsetpos(mf->backing, &(mf->lines[lineno]));
	return getline(line, n, mf->backing);
}

void refresh(const struct more_tty *mt, struct morefile *mf)
{
	char *line = NULL;
	size_t n = 0;
	for (size_t i = mf->topline; i < mf->topline + mt->lines; i++) {
		/* FIXME: account for long lines */

		if (more_getline(mf, i, &line, &n) == -1) {
			break;
		}

		printf("%s", line);
	}
	free(line);
}

void scroll(const struct more_tty *mt, struct morefile *mf, int count, int multiple)
{
	char *line = NULL;
	size_t n = 0;
	int by = count ? count * multiple : multiple;

	if (by < 0) {
		if ((size_t)(-by) > mf->topline) {
			mf->topline = 0;
		} else {
			mf->topline += by;
		}
		refresh(mt, mf);
	} else while (by-- > 0) {
		/* FIXME: account for long lines here, too */

		mf->topline++;
		more_getline(mf, mf->topline + mt->lines + 1, &line, &n);
		printf("%s", line);
	}

	free(line);
}

void mark(const struct more_tty *mt, struct morefile *mf)
{
	int c = fgetc(mt->tty);
	if (islower(c)) {
		mf->mark[c - 'a'] = mf->topline;
	}
}

void jump(const struct more_tty *mt, struct morefile *mf)
{
	int c = fgetc(mt->tty);
	if (islower(c)) {
		mf->topline = mf->mark[c - 'a'];
		refresh(mt, mf);
	}
}

int more(const struct more_tty *mt, const char *file)
{
	struct morefile mf = {
		.f = stdin,
	};

	int count = 0;

	if (strcmp(file, "-")) {
		mf.f = fopen(file, "r");
		if (!mf.f) {
			fprintf(stderr, "more: %s: %s\n", file, strerror(errno));
			retval = 1;
			return 1;
		}
	}

	fpos_t pos;
	if (fgetpos(mf.f, &pos) != 0) {
		mf.backing = tmpfile();
	} else {
		mf.backing = mf.f;
	}

	refresh(mt, &mf);
	while (mf.f) {
		int c = fgetc(mt->tty);

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
				if (count == 0) {
					count = mt->lines;
				}
				scroll(mt, &mf, count, 1);
				break;

			case 'b':
			case CTRL_B:
				if (count == 0) {
					count = mt->lines;
				}
				scroll(mt, &mf, count, -1);
				break;

			case ' ':
				count = count ? count : mt->lines;
				/* FALLTHRU */
			case 'j':
			case '\n':
				scroll(mt, &mf, count, 1);
				break;

			case 'k':
				scroll(mt, &mf, count, -1);
				break;

			case 'd':
			case CTRL_D:
				scroll(mt, &mf, count, mt->lines / 2);
				break;

			case 's':
				count = count ? count : 1;
				scroll(mt, &mf, mt->lines + count, 1);
				break;

			case 'u':
			case CTRL_U:
				scroll(mt, &mf, count, -mt->lines / 2);
				break;

			case 'g':
				//scroll_beginning(count);
				break;

			case 'G':
				//scroll_end(count);
				break;

			case 'r':
			case CTRL_L:
				refresh(mt, &mf);
				break;

			case 'R':
				// discard();
				refresh(mt, &mf);
				break;

			case 'm':
				mark(mt, &mf);
				break;

			case '\'':
				jump(mt, &mf);
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
				fputc(c, mt->tty);
				int c2 = fgetc(mt->tty);
				fprintf(mt->tty, "\b \b");
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
				if (fgetc(mt->tty) != 'Z') {
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

static void adjust_args(int *argc, char ***argv)
{
	char *env = getenv("MORE");
	if (env) {
		char **newargv = malloc((*argc + 2) * sizeof(*newargv));
		newargv[0] = (*argv)[0];
		
		/* TODO: account for spaces in env */
		newargv[1] = env;

		for (int i = 1; i < *argc; i++) {
			newargv[i + 1] = (*argv)[i];
		}

		newargv[*argc + 1] = NULL;

		*argv = newargv;
		(*argc)++;
	}
	
	for (int i = 1; i < *argc; i++) {
		if (!strcmp((*argv)[i], "--")) {
			return;
		}

		if ((*argv)[i][0] == '+') {
			fprintf(stderr, "more: adjusting command line option +%s to -%s\n", (*argv)[i] + 1, (*argv)[i] + 1);
			(*argv)[i][0] = '-';
		}
	}
}

static void cat_loop(FILE *f)
{
	int c = 0;
	while ((c = fgetc(f)) != EOF) {
		putchar(c);
	}
}

static void compress_loop(FILE *f)
{
	static int nl = 0;
	int c = 0;

	while ((c = fgetc(f)) != EOF) {
		if (c == '\n') {
			if (nl == 2) {
				continue;
			} else if (nl == 1) {
				nl = 2;
			} else {
				nl = 1;
			}
		} else {
			nl = 0;
		}
		putchar(c);
	}
}

static int more_cat(const char *path, void (*loop)(FILE *))
{
	FILE *f = stdin;
	if (path != NULL && strcmp(path, "-") != 0) {
		f = fopen(path, "r");
		if (!f) {
			fprintf(stderr, "more: %s: %s\n", path, strerror(errno));
			return 1;
		}
	}

	loop(f);

	if (f != stdin) {
		fclose(f);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int c;

	int clear = 0;
	int fastexit = 0;
	int ignorecase = 0;
	int compressempty = 0;
	int backspace = 1;
	int lines = 0;

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
			lines = atoi(optarg);
			break;

		case 'p':
			//perfile = optarg;
			break;

		case 't':
			//tag = optarg;
			break;

		default:
			return 1;
		}
	}

	if (!isatty(STDOUT_FILENO)) {
		int ret = 0;
		void (*loop)(FILE*) = compressempty ? compress_loop : cat_loop;
		do {
			ret |= more_cat(argv[optind++], loop);
		} while (optind < argc);
		return ret;
	}

	struct more_tty mt = more_open_tty(lines);

	if (optind >= argc) {
		more(&mt, "-");
	}

	int min = optind;
	int max = argc - 1;
	while (optind < argc) {
		int next = more(&mt, argv[optind]);
		optind += next;
		if (optind < min) {
			optind = min;
		}
		if (optind > max) {
			optind = max;
		}
	}

	(void)clear;
	(void)fastexit;
	(void)ignorecase;
	(void)backspace;

	return retval;
}
