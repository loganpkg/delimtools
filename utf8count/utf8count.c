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

/* utf8count -- counts UTF8 characters */

#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utf8.h"

#define INIT_BUF_SIZE BUFSIZ

/* Error message with source file and line number */
#define LOG(m) fprintf(stderr, "%s:%d: error: " m "\n", __FILE__, __LINE__)

int main(int argc, char **argv)
{
	int ret = 0;
	FILE *fp = NULL;
	char *buf = NULL;
	size_t buf_size;
	ssize_t line_len;
	size_t *count = NULL;
	uint32_t j;
	char utf8chstr[5] = {0};

	if (argc != 1 && argc != 2) {
		fprintf(stderr, "Usage: %s: [file]\n", argv[0]);
		return 1;
	}

	if (argc == 1 || !strcmp(argv[1], "-")) {
		fp = stdin;
	} else {
		if ((fp = fopen(argv[1], "r")) == NULL) {
			LOG("fopen failed");
			return 1;
		}
	}

	if ((count = calloc(NUMCP, sizeof(size_t))) == NULL) {
		LOG("calloc failed");
		ret = 1;
		goto clean_up;
	}

	if ((buf = malloc(INIT_BUF_SIZE)) == NULL) {
		LOG("malloc failed");
		ret = 1;
		goto clean_up;
	}

	buf_size = INIT_BUF_SIZE;

	while ((line_len = getline(&buf, &buf_size, fp)) > 0) {
		if (ucount(buf, line_len, count)) {
			ret = 1;
			goto clean_up;
		}
	}

	for (j = 0; j < NUMCP; ++j) {
		if (count[j]) {
			if (ucptostr(j, utf8chstr)) {
				ret = 1;
				goto clean_up;
			}
			if (iscntrl(utf8chstr[0]))
				printf("%u\t%02X\t%lu\n", j, utf8chstr[0], count[j]);
			else
				printf("%u\t%s\t%lu\n", j, utf8chstr, count[j]);
		}
	}

 clean_up:
	if (fp != NULL && fp != stdin) {
		if (fclose(fp)) {
			LOG("fclose failed");
			ret = 1;
		}
	}

	if (count != NULL) {
		free(count);
	}

	if (buf != NULL) {
		free(buf);
	}

	return ret;
}
