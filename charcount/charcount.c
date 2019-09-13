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
 * charcount -- counts chars
 * $ cc -ansi -O2 -g -Wall -Wextra -pedantic -o charcount charcount.c
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNKSIZE BUFSIZ

#define LOG(m) fprintf(stderr, "%s:%d: error: " m "\n", __FILE__, __LINE__)

#define PROCESS() do { \
		for (i = 0; i < num; ++i) { \
			++count[(unsigned char) a[i]]; \
		} \
} while (0)

int main(int argc, char **argv)
{
	int ret = 0;
	FILE *fp = NULL;
	char *a = NULL;
	size_t count[UCHAR_MAX] = { 0 };
	size_t num = 0;
	size_t i;
	int j;

	if (argc != 1 && argc != 2) {
		fprintf(stderr, "Usage: %s [file]\n", argv[0]);
		return 1;
	}
	if (argc == 1 || !strcmp(argv[1], "-")) {
		fp = stdin;
	} else if ((fp = fopen(argv[1], "r")) == NULL) {
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
		LOG("End of file not reached");
		ret = 1;
		goto clean_up;
	}
	if (num) {
		/* Process last paritial chunk */
		PROCESS();
	}
	for (j = 0; j < UCHAR_MAX; ++j) {
		if (count[j]) {
			if (isprint(j) && j != ' ')
				printf("%c\t%lu\n", j, count[j]);
			else
				printf("%02X\t%lu\n", j, count[j]);
		}
	}

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
