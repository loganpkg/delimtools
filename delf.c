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

#define _GNU_SOURCE
#include <sys/stat.h>
#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utf8.h"

#define INIT_BUF_SIZE 1024
#define MAX_LINES 100

int main(int argc, char **argv)
{
	int ret = 0;
	FILE *fp = NULL;
	struct stat st;
	char *buf = NULL;
	size_t buf_size = 0;
	ssize_t line_len;
	size_t *first_freq = NULL;
	size_t *sub_freq = NULL;
	int *eliminated = NULL;
	size_t num;
	size_t row_count;
	size_t i;
	uint32_t delim;
	size_t weight;
	int set;

	/* Set to the environment locale */
	if (setlocale(LC_CTYPE, "") == NULL) {
		LOGERR("setlocale failed");
		return 1;
	}

	/* This should be 4 for UTF-8 */
	if (MB_CUR_MAX != 1 && MB_CUR_MAX != 4) {
		if (setlocale(LC_CTYPE, "C") == NULL) {
			LOGERR("setlocale failed");
			return 1;
		}
	}

	if (MB_CUR_MAX == 4) {
		num = NUMCP;
	} else {
		num = UCHAR_MAX + 1;
	}

	if (argc != 1 && argc != 2) {
		fprintf(stderr, "Usage: %s: [file]\n", argv[0]);
		return 1;
	}

	if (argc == 1 || !strcmp(argv[1], "-")) {
		fp = stdin;
	} else {

		if (stat(argv[1], &st)) {
			LOGERR("stat failed");
			return 1;
		}

		if (!S_ISREG(st.st_mode)) {
			LOGERR("not a regular file");
			return 1;
		}

		if (st.st_size < 0) {
			LOGERR("negative file size");
			return 1;
		}

		if (st.st_size == 0) {
			return 1;
		}

		if ((fp = fopen(argv[1], "r")) == NULL) {
			LOGERR("fopen failed");
			return 1;
		}
	}

	if (MULTOF(NUMCP, sizeof(size_t))) {
		LOGERR("multiplication overflow");
		ret = 1;
		goto clean_up;
	}

	if (MULTOF(NUMCP, sizeof(int))) {
		LOGERR("multiplication overflow");
		ret = 1;
		goto clean_up;
	}

	first_freq = calloc(NUMCP, sizeof(size_t));

	if (first_freq == NULL) {
		LOGERR("calloc failed");
		ret = 1;
		goto clean_up;
	}

	sub_freq = calloc(NUMCP, sizeof(size_t));

	if (sub_freq == NULL) {
		LOGERR("calloc failed");
		ret = 1;
		goto clean_up;
	}

	eliminated = calloc(NUMCP, sizeof(int));

	if (eliminated == NULL) {
		LOGERR("calloc failed");
		ret = 1;
		goto clean_up;
	}

	buf = malloc(INIT_BUF_SIZE);

	if (buf == NULL) {
		LOGERR("malloc failed");
		ret = 1;
		goto clean_up;
	}

	buf_size = INIT_BUF_SIZE;

	row_count = 0;
	while ((line_len = getline(&buf, &buf_size, fp)) > 0
	       && row_count < MAX_LINES) {

		++row_count;

		if (row_count == 1) {
			if (MB_CUR_MAX == 4) {
				if (ufreq(buf, line_len, first_freq)) {
					ret = 1;
					goto clean_up;
				}
			} else {
				for (i = 0; i < (size_t) line_len; ++i) {
					++first_freq[(int)buf[i]];
				}
			}
			for (i = 0; i < num; ++i) {
				if (first_freq[i] == 0) {
					eliminated[i] = 1;
				}
			}
		} else {
			if (MB_CUR_MAX == 4) {
				if (ufreq(buf, line_len, sub_freq)) {
					ret = 1;
					goto clean_up;
				}
			} else {
				for (i = 0; i < (size_t) line_len; ++i) {
					++sub_freq[(int)buf[i]];
				}
			}
			for (i = 0; i < num; ++i) {
				if (sub_freq[i] != first_freq[i]) {
					eliminated[i] = 1;
				}
			}
			for (i = 0; i < num; ++i) {
				sub_freq[i] = 0;
			}
		}
	}

	set = 0;
	weight = 0;
	for (i = 0; i < num; ++i) {
		if (!eliminated[i] && first_freq[i] > weight && i != '\n') {
			delim = i;
			set = 1;
			weight = first_freq[i];
		}
	}

	if (!set) {
		ret = 1;
		goto clean_up;
	}

	if (delim == 9) {
		printf("\\t");
	} else if (uprintcp(delim)) {
		ret = 1;
		goto clean_up;
	}
	putchar('\n');

 clean_up:
	if (fp != NULL && fp != stdin) {
		if (fclose(fp)) {
			LOGERR("fclose failed");
			ret = 1;
		}
	}

	if (first_freq != NULL) {
		free(first_freq);
	}

	if (sub_freq != NULL) {
		free(sub_freq);
	}

	if (eliminated != NULL) {
		free(eliminated);
	}

	if (buf != NULL) {
		free(buf);
	}

	return ret;
}
