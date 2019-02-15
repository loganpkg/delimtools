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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOGERR(m) (void) fprintf(stderr, "%s:%d: error: " m "\n", __FILE__, __LINE__)

#define INIT_BUF_SIZE 1024
#define NUM_CH UCHAR_MAX + 1
#define MAX_LINES 100

int main(int argc, char **argv)
{
	int ret = 0;
	FILE *fp = NULL;
	struct stat st;

	char *buf = NULL;
	size_t buf_size = 0;
	ssize_t line_len;

	size_t first_freq[NUM_CH] = { 0 };
	size_t sub_freq[NUM_CH] = { 0 };
	int eliminated[NUM_CH] = { 0 };

	size_t row_count;
	size_t i;
	int delim;
	size_t weight;

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
			for (i = 0; i < (size_t) line_len; ++i) {
				++first_freq[(int)buf[i]];
			}
			for (i = 0; i < NUM_CH; ++i) {
				if (first_freq[i] == 0) {
					eliminated[i] = 1;
				}
			}
		} else {
			for (i = 0; i < (size_t) line_len; ++i) {
				++sub_freq[(int)buf[i]];
			}
			for (i = 0; i < NUM_CH; ++i) {
				if (sub_freq[i] != first_freq[i]) {
					eliminated[i] = 1;
				}
			}
			for (i = 0; i < NUM_CH; ++i) {
				sub_freq[i] = 0;
			}
		}
	}

	delim = -1;
	weight = 0;
	for (i = 0; i < NUM_CH; ++i) {
		if (!eliminated[i] && first_freq[i] > weight && i != '\n') {
			delim = i;
			weight = first_freq[i];
		}
	}

	if (delim == -1) {
		ret = 1;
		printf("%d\n", delim);
	} else {
		if (isprint(delim)) {
			printf("%c\n", delim);
		} else {
			switch (delim) {
			case '\0':
				printf("\\0\n");
				break;
			case '\t':
				printf("\\t\n");
				break;
			default:
				printf("%02x\n", (unsigned char)delim);
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

	if (buf != NULL) {
		free(buf);
	}

	return ret;
}
