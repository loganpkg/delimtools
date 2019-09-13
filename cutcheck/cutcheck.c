/*
 * Copyright (c) 2019 Logan Ryan McLintock
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * cutcheck -- checks the delimiter of a file for use with cut
 * $ cc -ansi -O2 -g -Wall -Wextra -pedantic -o cutcheck cutcheck.c
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNKSIZE BUFSIZ

#define LOG(m) fprintf(stderr, "%s:%d: error: " m "\n", __FILE__, __LINE__)

#define COMPARE() do {						\
	if (count != first_count) {				\
		fprintf(stderr, "%s:%s:%lu: error: "		\
		"inconsistent delimiter, "			\
		"expected %lu, found %lu\n",			\
		argv[0], argv[2], row, first_count, count);	\
		ret = 1;					\
		goto clean_up;					\
	}							\
} while (0)

#define PROCESS() do {					\
	for (i = 0; i < num; ++i) {			\
		if (a[i] == delim)			\
			++count;			\
		if (a[i] == '\n') {			\
			if (body) {			\
				COMPARE();		\
			} else {			\
				first_count = count;	\
				body = 1;		\
			}				\
			++row;				\
			count = 0;			\
		}					\
	}						\
} while (0)

int main(int argc, char **argv)
{
	int ret = 0;
	FILE *fp = NULL;
	char *a = NULL;
	char delim;
	size_t num = 0;
	int body = 0;
	size_t count = 0;
	size_t first_count = 0;
	size_t row = 1;
	size_t i;

	if (argc != 2 && argc != 3) {
		fprintf(stderr, "usage: %s delimiter [file]\n", argv[0]);
		return 1;
	}
	if (!strcmp(argv[1], "\\t")) {
		delim = '\t';
	} else if (!strcmp(argv[1], "\\0")) {
		delim = '\0';
	} else if (strlen(argv[1]) == 1) {
		delim = argv[1][0];
	} else {
		LOG("delimiter must be one character");
		return 1;
	}

	if (argc == 2 || !strcmp(argv[2], "--")) {
		fp = stdin;
	} else if ((fp = fopen(argv[2], "r")) == NULL) {
		LOG("fopen failed");
		return 1;
	}

	if ((a = malloc(CHUNKSIZE)) == NULL) {
		LOG("malloc failed");
		ret = 1;
		goto clean_up;
	}
	while ((num = fread(a, 1, CHUNKSIZE, fp)) == CHUNKSIZE) {
		/* Process full chunks */
		PROCESS();
	}

	/* Short or zero read */
	if (ferror(fp)) {
		LOG("fread failed");
		ret = 1;
		goto clean_up;
	}
	if (!feof(fp)) {
		LOG("end of file not reached");
		ret = 1;
		goto clean_up;
	}
	if (num) {
		/* Process last paritial chunk */
		PROCESS();
	}
	if (a[num - 1] != '\n' && body) {
		/* File has no final line feed and is not a single line */
		COMPARE();
	}
	if (!count && !first_count)
		fprintf(stderr,
			"%s:%s: warning: no delimiter "
			"characters were found\n", argv[0], argv[2]);

 clean_up:
	if (fp != NULL && fp != stdin) {
		if (fclose(fp)) {
			LOG("fclose failed");
			ret = 1;
		}
	}

	free(a);

	return ret;
}
