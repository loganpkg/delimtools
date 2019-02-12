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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOGERR(m) (void) fprintf(stderr, "%s:%d: error: " m "\n", __FILE__, __LINE__)

#define INIT_BUF_SIZE 1024

int main(int argc, char **argv)
{
	int ret = 0;
	FILE *fp = NULL;
	char *buf = NULL;
	size_t buf_size = 0;
	ssize_t line_len;
	size_t row_count;
	size_t i;
	size_t delim_count;
	size_t first_delim_count;

	struct stat st;
	char delim;

	if (argc != 2 && argc != 3) {
		fprintf(stderr, "Usage: %s: delimiter [file]\n", argv[0]);
		return 1;
	}

	if (strnlen(argv[1], 2) != 1) {
		LOGERR("delimiter must be one character");
		return 1;
	}

	delim = argv[1][0];

	if (delim == '\n') {
		LOGERR("delimiter cannot a \\n character");
		return 1;
	}

	if (argc == 2) {
		fp = stdin;
	} else {

		if (stat(argv[2], &st)) {
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
			return 0;
		}

		if ((fp = fopen(argv[2], "r")) == NULL) {
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
	delim_count = 0;

	while ((line_len = getline(&buf, &buf_size, fp)) > 0) {
		++row_count;

		for (i = 0; i < (size_t) line_len; ++i) {
			if (buf[i] == delim) {
				++delim_count;
			}
		}

		if (row_count == 1) {
			first_delim_count = delim_count;

		} else if (delim_count != first_delim_count) {
			fprintf(stderr,
				"%s: Delimiter mismatch:\nLine 1: %lu\nLine %lu: %lu\n",
				argv[0], first_delim_count, row_count,
				delim_count);
			ret = 1;
			goto clean_up;
		}

		delim_count = 0;
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
