#define _XOPEN_SOURCE 700
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "more.h"

ssize_t more_getline(struct more_file *mf, size_t lineno)
{
	if (mf->nlines <= lineno && mf->nlines != 0) {
		fsetpos(mf->f, &(mf->lines[mf->nlines - 1]));
		getline(&(mf->buf), &(mf->nbuf), mf->f);
	}

	while (mf->nlines <= lineno) {
		mf->nlines++;
		mf->lines = realloc(mf->lines, mf->nlines * sizeof(*mf->lines));
		mf->bytepos = realloc(mf->bytepos, mf->nlines * sizeof(*mf->bytepos));

		fgetpos(mf->f, &(mf->lines[mf->nlines - 1]));

		getline(&(mf->buf), &(mf->nbuf), mf->f);
		if (mf->nlines > 1) {
			mf->bytepos[mf->nlines - 1] = mf->bytepos[mf->nlines - 2] + strlen(mf->buf);
		} else {
			mf->bytepos[0] = 0;
		}

		if (mf->backing != mf->f) {
			fgetpos(mf->backing, &(mf->lines[mf->nlines - 1]));
			fputs(mf->buf, mf->backing);
		}
	}

	fsetpos(mf->backing, &(mf->lines[lineno]));
	return getline(&(mf->buf), &(mf->nbuf), mf->backing);
}

struct more_file more_open(const char *path)
{
	struct more_file mf = {
		.f = stdin,
	};

	if (strcmp(path, "-")) {
		mf.f = fopen(path, "r");
		if (!mf.f) {
			fprintf(stderr, "more: %s: %s\n", path, strerror(errno));
			return mf;
		}
	}

	fpos_t pos;
	if (fgetpos(mf.f, &pos) != 0) {
		mf.backing = tmpfile();
	} else {
		mf.backing = mf.f;
		struct stat st;
		fstat(fileno(mf.f), &st);
		mf.nbytes = st.st_size;
	}

	return mf;
}

void more_close(struct more_file *mf)
{
	if (mf->backing != mf->f) {
		fclose(mf->backing);
	}

	if (mf->f != stdin) {
		fclose(mf->f);
	}

	free(mf->lines);
	free(mf->buf);
}
