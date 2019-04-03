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

#include <macros.h>
#include <utf8.h>

#define INIT_BUF_SIZE 1024

int main(int argc, char **argv)
{
	int ret = 0;
	FILE *fp = NULL;
	struct stat st;
	char *buf = NULL;
	size_t buf_size;
	ssize_t line_len;
	size_t *freq = NULL;
	ssize_t i;
	uint32_t j;

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

	freq = calloc(NUMCP, sizeof(size_t));

	if (freq == NULL) {
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

	while ((line_len = getline(&buf, &buf_size, fp)) > 0) {

		if (MB_CUR_MAX == 4) {
			if (ufreq(buf, line_len, freq)) {
				ret = 1;
				goto clean_up;
			}
		} else {
			for (i = 0; i < line_len; ++i) {
				++freq[(unsigned char)buf[i]];
			}
		}
	}

	if (MB_CUR_MAX == 4) {
		for (j = 0; j < NUMCP; ++j) {
			if (freq[j]) {
				printf("%u\t", j);

				if (j == 9) {
					printf("\\t");
				} else if (j == 10) {
					printf("\\n");
				} else if (uprintcp(j)) {
					ret = 1;
					goto clean_up;
				}

				printf("\t%lu\n", freq[j]);
			}
		}
	} else {
		for (j = 0; j <= UCHAR_MAX; ++j) {
			if (freq[j]) {
				printf("%u\t", j);

				if (j == 9) {
					printf("\\t");
				} else if (j == 10) {
					printf("\\n");
				} else {
					printf("%c", (char)j);
				}

				printf("\t%lu\n", freq[j]);
			}
		}
	}

 clean_up:
	if (fp != NULL && fp != stdin) {
		if (fclose(fp)) {
			LOGERR("fclose failed");
			ret = 1;
		}
	}

	if (freq != NULL) {
		free(freq);
	}

	if (buf != NULL) {
		free(buf);
	}

	return ret;
}
